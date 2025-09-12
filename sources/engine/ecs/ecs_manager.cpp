#include "pch.h"
#include <nasral/ecs/ecs_manager.h>
#include <nasral/engine.h>

namespace nasral::ecs
{

}

namespace nasral::ecs
{
    EcsManager::EcsManager(const Engine* engine, const EcsConfig& config)
        : engine_(engine)
        , config_(config)
    {
        entities_.reserve(config_.max_entities);
        freed_slots_.reserve(config_.max_entities);
    }

    const logging::Logger* EcsManager::logger() const{
        return engine_->logger();
    }

    Archetype* EcsManager::find_or_create_archetype(const ComponentMask& mask){
        for (const auto& archetype : archetypes_){
            if (archetype->mask() == mask){
                return archetype.get();
            }
        }

        archetypes_.emplace_back(std::make_unique<Archetype>(mask, config_.max_entities));
        return archetypes_.back().get();
    }

    EntityId EcsManager::create_entity(){
        if (!freed_slots_.empty()){
            const auto index = freed_slots_.back();
            freed_slots_.pop_back();
            const auto& slot = entities_[index];
            return slot.id;
        }

        if (entities_.size() >= config_.max_entities){
            throw EcsError("No free entity slots available");
        }

        EntitySlot slot;
        slot.id = EntityId{entities_.size(), 0};
        slot.is_alive = true;
        entities_.emplace_back(slot);
        return slot.id;
    }

    void EcsManager::destroy_entity(const EntityId& id){
        if (id.index >= entities_.size()
            || id.version != entities_[id.index].id.version
            || !entities_[id.index].is_alive)
        {
            return;
        }

        auto& slot = entities_[id.index];

        // Удалить из текущего архетипа
        const auto result = slot.archetype->remove_entity(slot.archetype_index);

        // Если было перемещение при удалении в архетипе (swap & pop),
        // тот, что встал на место удаленного, теперь обладает индексом удаленного
        if (result.moved_entity.has_value()){
            assert(result.prev_arch_index.has_value());
            const auto& e = result.moved_entity.value();
            entities_[e.index].archetype_index = result.prev_arch_index.value();
        }

        // Очистить слот
        slot.archetype = nullptr;
        slot.archetype_index = 0;
        slot.is_alive = false;
        slot.id.version++;

        // Добавить в список освобожденных слотов
        freed_slots_.push_back(id.index);
    }

    void EcsManager::assign_archetype(EntitySlot &slot, Archetype *dst_archetype) {
        // Если это новая сущность (еще нет архетипа)
        if (!slot.archetype)
        {
            slot.archetype = dst_archetype;
            slot.archetype_index = dst_archetype->add_entity(slot.id);
            slot.mask = dst_archetype->mask();
        }
        // Если entity не новая (принадлежит другому архетипу) - переместить
        else if (slot.archetype != dst_archetype)
        {
            // Прежний архетип
            auto* prev_archetype = slot.archetype;

            // На случай перемещения другой entity (swap & pop) при удалении из прежнего архетипа
            const auto result = dst_archetype->move_entity_from(prev_archetype, slot.id);
            if (result.moved_entity.has_value()){
                auto& other = entities_[result.moved_entity.value().index];
                other.archetype_index = result.prev_arch_index.value();
            }

            // Задать новый архетип и индекс в нем
            slot.archetype = dst_archetype;
            slot.archetype_index = result.new_arch_index;
            slot.mask = dst_archetype->mask();

            // Если в прежнем архетипе не осталось сущностей - удалить архетип
            if (prev_archetype->entities().empty()) {
                const auto should_remove = std::remove_if(archetypes_.begin(), archetypes_.end(), [&](const Archetype::Ptr& a) {
                    return a.get() == prev_archetype;
                });
                archetypes_.erase(should_remove, archetypes_.end());
            }
        }
    }

    void EcsManager::enable_components(const EntityId& id, const ComponentMask& mask){
        assert(id.index < entities_.size());

        // Получение слота по entity
        auto& slot = entities_[id.index];
        if (slot.is_alive == false || slot.id.version != id.version) {
            throw EcsError("Entity is invalid");
        }

        // Новая маска компонентов (с добавленными)
        ComponentMask new_mask = slot.mask | mask;
        if (new_mask == slot.mask) {
            return;
        }

        // Получить архетип по маске
        auto* dst_archetype = find_or_create_archetype(new_mask);

        // Перемещение в архетип
        assign_archetype(slot, dst_archetype);
    }

    void EcsManager::disable_components(const EntityId& id, const ComponentMask& mask){
        assert(id.index < entities_.size());

        // Получение слота по entity
        auto& slot = entities_[id.index];
        if (slot.is_alive == false || slot.id.version != id.version) {
            throw EcsError("Entity is invalid");
        }

        // Новая маска компонентов (с исключёнными)
        ComponentMask new_mask = slot.mask & ~mask;
        if (new_mask == slot.mask) {
            return;
        }

        // Получить архетип по маске
        auto* dst_archetype = find_or_create_archetype(new_mask);

        // Перемещение в архетип
        assign_archetype(slot, dst_archetype);
    }
}
