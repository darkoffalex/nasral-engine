#include "pch.h"
#include <nasral/ecs/ecs_manager.h>
#include <nasral/engine.h>

namespace nasral::ecs
{
    Archetype::Archetype(const ComponentMask& mask, const size_t max_entities)
        : mask_(mask)
        , max_entities_(max_entities)
    {
        entities_.reserve(max_entities);
        pools_.reserve(std::tuple_size_v<ComponentTypes>);
        component_types_.reserve(std::tuple_size_v<ComponentTypes>);

        std::apply([&]([[maybe_unused]] auto... component_dummies) {
            [[maybe_unused]] size_t comp_id = 0;
            ([&]{
                if (mask.test(comp_id)) {
                    using ComponentType = decltype(component_dummies);
                    pools_.emplace_back(std::vector<ComponentType>{});
                    std::get<std::vector<ComponentType>>(pools_.back()).reserve(max_entities);
                    component_types_.emplace_back(comp_id);
                }
                ++comp_id;
            }(), ...);
        }, ComponentTypes{});
    }

    size_t Archetype::add_entity(const EntityId& id){
        if (entities_.size() >= max_entities_){
            throw EcsError("No free entity slots available");
        }

        // Добавить entity в список
        entities_.push_back(id);
        // Добавить значения компонентов по умолчанию (должны быть default constructible)
        for (auto& pool : pools_) {
            std::visit([](auto& components){
                components.emplace_back();
            }, pool);
        }
        // Вернуть индекс добавленной entity
        return entities_.size() - 1;
    }

    Archetype::MoveResult Archetype::move_entity_from(Archetype* from, const EntityId& id){
        if (entities_.size() >= max_entities_){
            throw EcsError("No free entity slots available");
        }

        // Добавить entity, получив новый индекс (пустые компоненты).
        // Получить индекс entity в старом archetype.
        MoveResult result;
        result.new_arch_index = add_entity(id);
        result.prev_arch_index = from->get_entity_index(id);

        // Обработать компоненты, которые есть в обоих архетипах (перенос данных)
        const ComponentMask intersection_components = mask_ & from->mask_;
        std::apply([&]([[maybe_unused]] auto... component_dummies) {
            [[maybe_unused]] size_t comp_id = 0;
            ([&] {
                if (intersection_components.test(comp_id)) {
                    using ComponentType = decltype(component_dummies);
                    const size_t src_pool_idx = from->get_pool_index(comp_id);
                    const size_t dst_pool_idx = get_pool_index(comp_id);
                    auto& src_vec = std::get<std::vector<ComponentType>>(from->pools_[src_pool_idx]);
                    auto& dst_vec = std::get<std::vector<ComponentType>>(pools_[dst_pool_idx]);
                    dst_vec[*result.new_arch_index] = std::move(src_vec[*result.prev_arch_index]);
                }
                ++comp_id;
            }(), ...);
        }, ComponentTypes{});

        // Удалить entity из исходного архетипа (компоненты также удаляются).
        // Удаление происходит посредством "swap & pop", элементы могут быть перемещены
        const auto rr = from->remove_entity(result.prev_arch_index.value());
        result.moved_entity = rr.moved_entity;

        // Вернуть индекс добавленной entity
        return result;
    }

    Archetype::MoveResult Archetype::remove_entity(const size_t index){
        if (index >= entities_.size()){
            return {};
        }

        // Если перемещение (swap & pop) было, результат будет хранить информацию об этом
        MoveResult result = {};

        // Swap-pop (обменять с последним)
        size_t last_idx = entities_.size() - 1;
        if (index != last_idx) {
            std::swap(entities_[index], entities_[last_idx]);
            for (auto& pool : pools_) {
                std::visit([index, last_idx](auto& components) {
                    components[index] = std::move(components[last_idx]);
                }, pool);
            }

            result.moved_entity = entities_[index];
            result.prev_arch_index = index;
        }

        // Очистить entity и компоненты (автоматически вызовет деструкторы)
        entities_.pop_back();
        for (auto& pool : pools_) {
            std::visit([](auto& vec) { vec.pop_back(); }, pool);
        }

        return result;
    }

    size_t Archetype::get_pool_index(const size_t component_id) const{
        const auto it = std::find(component_types_.begin(), component_types_.end(), component_id);
        if (it == component_types_.end()){
            throw EcsError("Component not found");
        }
        return std::distance(component_types_.begin(), it);
    }

    size_t Archetype::get_entity_index(const EntityId& id) const{
        const auto it = std::find(entities_.begin(), entities_.end(), id);
        if (it == entities_.end()){
            throw EcsError("Entity not found");
        }
        return std::distance(entities_.begin(), it);
    }
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

    void EcsManager::assign_components(const EntityId& id, const ComponentMask& mask){

    }

    void EcsManager::remove_components(const EntityId& id, const ComponentMask& mask){

    }

            /*
        template<typename Component>
        void assign_component(const EntityId& id, const Component&& component){
            assert(id.index < entities_.size());
            auto& slot = entities_[id.index];

            if (!slot.is_alive){
                throw EcsError("Entity is not alive");
            }
            if (slot.id.version != id.version){
                throw EcsError("Entity version mismatch");
            }

            // Маска компонентов включая новый
            ComponentMask mask = slot.mask;
            mask.set(kComponentId<Component>);

            // Получить архетип по маске
            auto* dst_archetype = find_or_create_archetype(mask);

            // Если это новая сущность (еще нет архетипа)
            if (!slot.archetype){
                slot.archetype = dst_archetype;
                slot.archetype_index = dst_archetype->add_entity(id);
                slot.mask = mask;
            }
            // Если entity не новая (принадлежит другому архетипу) - переместить
            else if (slot.archetype != dst_archetype){
                // Если было перемещение (swap & pop) - обновить данные перемещенной entity
                const auto result = dst_archetype->move_entity_from(slot.archetype, id);
                if (result.moved_entity.has_value()){
                    auto& e = entities_[result.moved_entity.value().index];
                    e.archetype_index = result.prev_arch_index.value();
                }
                // Задать новый архетип и индекс в нем
                slot.archetype_index = result.new_arch_index.value();
                slot.archetype = dst_archetype;
                slot.mask = mask;
            }

            // Задать значение компонента
            assert(dst_archetype->has_component<Component>());
            dst_archetype->get_component<Component>(slot.archetype_index) = std::forward<Component>(component);
        }

        template<typename Component>
        void remove_component(const EntityId& id){
            assert(id.index < entities_.size());
            assert(entities_[id.index].archetype != nullptr);
            auto& slot = entities_[id.index];

            if (!slot.is_alive){
                throw EcsError("Entity is not alive");
            }
            if (slot.id.version != id.version){
                throw EcsError("Entity version mismatch");
            }
            if (slot.archetype->has_component<Component>() == false){
                throw EcsError("Entity does not have component");
            }

            // Новая маска компонентов (без удаленного)
            ComponentMask mask = slot.mask;
            mask.set(kComponentId<Component>, false);

            // Если больше нет компонентов - удалить из архетипа (компоненты также удаляются)
            if (mask.none()){
                auto result = slot.archetype->remove_entity(slot.archetype_index);
                if (result.moved_entity.has_value()){
                    auto& e = entities_[result.moved_entity.value().index];
                    e.archetype_index = result.prev_arch_index.value();
                }

                slot.archetype_index = 0;
                slot.archetype = nullptr;
                slot.mask = {};
            }
            // В ином случае - переместить из прежнего архетипа в новый
            else{
                auto* dst_archetype = find_or_create_archetype(mask);
                auto result = dst_archetype->move_entity_from(slot.archetype, id);
                if (result.moved_entity.has_value()){
                    auto& e = entities_[result.moved_entity.value().index];
                    e.archetype_index = result.prev_arch_index.value();
                }

                slot.archetype_index = result.new_arch_index.value();
                slot.archetype = dst_archetype;
                slot.mask = mask;
            }
        }
        */
}
