#include "pch.h"
#include <nasral/ecs/ecs_archetype.h>

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
                    dst_vec[result.new_arch_index] = std::move(src_vec[*result.prev_arch_index]);
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