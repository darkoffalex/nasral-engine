#include "pch.h"
#include <nasral/ecs/archetype.h>

namespace nasral::ecs
{
    Archetype::Archetype(const ComponentMask& mask, const size_t max_entities)
        : max_entities_(max_entities)
        , mask_(mask)
    {
        // Резервировать память под entity
        entities_.reserve(max_entities_);
        // Зарезервировать память под максимальное кол-во пулов и типов
        pools_.reserve(std::tuple_size_v<ComponentTypes>);
        component_types_.reserve(std::tuple_size_v<ComponentTypes>);

        // Fold expression.
        // Пройтись по всем типам компонентов, добавить нужные пулы (под каждый используемый тип)
        // Зарезервировать нужное кол-во памяти в пуле (векторе) под кол-во entities
        std::apply([&]([[maybe_unused]] auto... component_dummies){
            [[maybe_unused]] size_t comp_idx = 0;
            ([&]{
                if (mask_.test(comp_idx)){
                    using ComponentType = decltype(component_dummies);
                    pools_.emplace_back(std::vector<ComponentType>());
                    auto& vec = std::get<std::vector<ComponentType>>(pools_.back());
                    vec.reserve(max_entities_);
                    component_types_.emplace_back(comp_idx);
                }
                ++comp_idx;
            }(), ...);
        }, ComponentTypes{});
    }

    Archetype::Result Archetype::add(const EntityId& entity_id)
    {
        if (entities_.size() >= max_entities_){
            throw std::runtime_error("Cannot add entity to archetype. Max entities reached.");
        }

        // Добавить entity
        entities_.push_back(entity_id);

        // Добавить необходимые компоненты (должны быть default-constructible)
        for (auto& pool : pools_){
            std::visit([&](auto& vec){
                vec.emplace_back();
            }, pool);
        }

        // В результате возвращается только индекс добавленной entity
        Result r{};
        r.new_idx = entities_.size() - 1;
        return r;
    }

    Archetype::Result Archetype::remove(const EntityId& entity_id)
    {
        const auto entity_idx = entity_index(entity_id);
        return remove(entity_idx);
    }

    Archetype::Result Archetype::remove(size_t entity_index)
    {
        if (entity_index >= entities_.size()){
            return Result{};
        }

        // Результат (может хранить данные о переносе, в случае swap & pop)
        Result r{};

        // Swap & pop, если удаляется из середины
        size_t last_entity_idx = entities_.size() - 1;
        if (last_entity_idx != entity_index){
            std::swap(entities_[entity_index], entities_[last_entity_idx]);
            for (auto& pool : pools_){
                std::visit([&](auto& vec){
                    std::swap(vec[entity_index], vec[last_entity_idx]);
                }, pool);
            }

            r.prev_idx = entity_index;
            r.swapped_entity = entities_[entity_index];
        }

        // Удаление последнего элемента (entity и компонентов)
        entities_.pop_back();
        for (auto& pool : pools_){
            std::visit([&](auto& vec){
                vec.pop_back();
            }, pool);
        }

        return r;
    }

    Archetype::Result Archetype::move_from(Archetype* src, const EntityId& entity_id)
    {
        if (entities_.size() >= max_entities_){
            throw std::runtime_error("Cannot add entity to archetype. Max entities reached.");
        }

        // Добавление entity в текущий архетип
        const auto added = add(entity_id);
        assert(added.new_idx.has_value() && "New entity ID is empty");

        // Результат переноса
        Result r{};
        r.new_idx = added.new_idx.value();
        r.prev_idx = src->entity_index(entity_id);

        // Перенос компонентов между архетипами
        std::apply([&]([[maybe_unused]] auto... component_dummies){
            [[maybe_unused]] size_t comp_idx = 0;
            ([&]{
                if (mask_.test(comp_idx++)){
                    // Если в исходном архетипе (откуда перенос) есть компонент
                    if (src->mask_.test(comp_idx)){
                        using ComponentType = decltype(component_dummies);
                        const size_t src_pool_idx = src->pool_index(comp_idx);
                        const size_t dst_pool_idx = pool_index(comp_idx);
                        auto& src_vec = std::get<std::vector<ComponentType>>(src->pools_[src_pool_idx]);
                        auto& dst_vec = std::get<std::vector<ComponentType>>(pools_[dst_pool_idx]);
                        dst_vec[r.new_idx.value()] = std::move(src_vec[r.prev_idx.value()]);
                    }
                }
                ++comp_idx;
            }(), ...);
        }, ComponentTypes{});

        // Удалить entity из исходного архетипа.
        // Сохранить информацию о перемещенной entity (для дальнейшего обновления информации в менеджере)
        const auto removed = src->remove(r.prev_idx.value());
        r.swapped_entity = removed.swapped_entity;

        return r;
    }

    /**
     * @brief Получение индекса пула по типу компонента
     * @param component_type Индекс типа компонента
     * @return Индекс соответствующего пула
     */
    size_t Archetype::pool_index(const size_t component_type) const{
        const auto it = std::find(component_types_.begin(), component_types_.end(), component_type);
        assert(it != component_types_.end() && "Component type is not present in the archetype component types");
        return std::distance(component_types_.begin(), it);
    }

    /**
     * @brief Получение индекса entity в списке по EntityId
     * @param entity_id Структура EntityID
     * @return Индекс соответствующей entity
     */
    size_t Archetype::entity_index(const EntityId& entity_id) const{
        const auto it = std::find(entities_.begin(), entities_.end(), entity_id);
        assert(it != entities_.end() && "Entity is not present in the archetype entity list");
        return std::distance(entities_.begin(), it);
    }
}
