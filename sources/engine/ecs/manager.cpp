#include "pch.h"
#include <nasral/ecs/manager.h>

namespace nasral::ecs
{
    Manager::Manager(Engine* engine, const Config& config)
        : Subsystem(engine, config)
    {
        entities_.reserve(config.max_entities);
        free_slots_.reserve(config.max_entities);
    }

    EntityId Manager::spawn()
    {
        // Если есть освобожденные слоты
        if (!free_slots_.empty()){
            const auto index = free_slots_.back();
            free_slots_.pop_back();

            auto& slot = entities_[index];
            slot.is_alive = true;
            return slot.id;
        }

        if (entities_.size() >= config().max_entities){
            throw std::runtime_error("Cannot create entity. Max entities reached.");
        }

        // Новый слот
        EntitySlot slot{};
        slot.id = EntityId{entities_.size(), 0};
        entities_.emplace_back(slot);
        return slot.id;
    }

    void Manager::destroy(const EntityId& entity_id)
    {
        if (entity_id.index >= entities_.size()
            || !entities_[entity_id.index].is_alive
            || entity_id.version != entities_[entity_id.index].id.version) return;

        auto& slot = entities_[entity_id.index];

        // Удаление из архетипа (возможен swap & pop)
        const auto rr = slot.archetype->remove(entity_id);

        // Если при удалении было перемещение внутри архетипа (у какой-то entity сменился индекс):
        // Нужно обновить поле "индекс внутри архетипа" у соответствующего слота
        if (rr.swapped_entity.has_value()){
            assert(rr.prev_idx.has_value());
            const auto& [index, version] = rr.swapped_entity.value();
            entities_[index].index_in_archetype = rr.prev_idx.value();
        }

        // Очистка слота
        slot.is_alive = false;
        slot.archetype = nullptr;
        slot.index_in_archetype = 0;
        slot.id.version++;

        // Добавить в список освобожденных слотов
        free_slots_.push_back(entity_id.index);
    }

    void Manager::destroy_deferred(const EntityId& entity_id){
        deferred_actions_.emplace_back([entity_id](Manager& m){
            m.destroy(entity_id);
        });
    }

    void Manager::apply_deferred_actions(){
        if (deferred_actions_.empty()) return;
        for (auto& action : deferred_actions_){
            action(*this);
        }
        deferred_actions_.clear();
    }

    Archetype* Manager::find_or_create_archetype(const ComponentMask& mask){
        for (const auto& archetype : archetypes_){
            if (archetype->mask() == mask){
                return archetype.get();
            }
        }
        archetypes_.emplace_back(std::make_unique<Archetype>(mask, config().max_entities));
        return archetypes_.back().get();
    }

    void Manager::assign_archetype(EntitySlot& slot, Archetype* archetype)
    {
        // Архетип отсутствует (новая entity)
        if (!slot.archetype)
        {
            const auto ar = archetype->add(slot.id);
            assert(ar.new_idx.has_value());

            slot.archetype = archetype;
            slot.index_in_archetype = ar.new_idx.value();
            slot.mask = archetype->mask();
        }
        // Уже принадлежит другому архетипу
        else if (slot.archetype != archetype)
        {
            // Прежний архетип
            auto* prev_arc = slot.archetype;

            // Перемещение между архетипами
            const auto& [new_idx, prev_idx, swapped] = archetype->move_from(prev_arc, slot.id);

            // Если внутри архетипа была перестановка (swap & pop)
            if (swapped.has_value()){
                assert(prev_idx.has_value());
                auto& other = entities_[swapped.value().index];
                other.index_in_archetype = prev_idx.value();
            }

            // Новый архетип и индекс в нем
            slot.archetype = archetype;
            slot.index_in_archetype = new_idx.value();
            slot.mask = archetype->mask();

            // Если в прежнем архетипе не осталось сущностей - удалить архетип
            if (prev_arc->entities().empty()){
                archetypes_.erase(std::remove_if(archetypes_.begin(), archetypes_.end(), [&](const Archetype::Ptr& a){
                    return a.get() == prev_arc;
                }), archetypes_.end());
            }
        }
    }
}
