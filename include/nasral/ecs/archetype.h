#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <nasral/ecs/entity.h>
#include <nasral/ecs/types.h>

namespace nasral::ecs
{
    class Archetype
    {
    public:
        typedef std::unique_ptr<Archetype> Ptr;

        /**
         * @brief Удаление и перенос между архетипами осуществляется при помощи swap-&-pop.
         * По этой причине, иногда необходимо знать индекс элемента в прежнем архетипе,
         * чтобы корректно завершить операции.
         */
        struct Result
        {
            std::optional<size_t> new_idx = std::nullopt;
            std::optional<size_t> prev_idx = std::nullopt;
            std::optional<EntityId> swapped_entity = std::nullopt;
        };

        Archetype(const ComponentMask& mask, size_t max_entities);

        Result add(const EntityId& entity_id);
        Result remove(const EntityId& entity_id);
        Result remove(size_t entity_index);
        Result move_from(Archetype* src, const EntityId& entity_id);

        [[nodiscard]] size_t size() const noexcept { return entities_.size(); }
        [[nodiscard]] const ComponentMask& mask() const noexcept { return mask_; }
        [[nodiscard]] const std::vector<EntityId>& entities() const noexcept { return entities_; }

        template<typename ComponentType>
        [[nodiscard]] bool has_component() const noexcept{
            const size_t component_id = kComponentId<ComponentType>;
            return mask_.test(component_id);
        }

        template<typename ComponentType>
        [[nodiscard]] ComponentType& get_component(size_t entity_index){
            const size_t comp_idx = kComponentId<ComponentType>;
            const size_t pool_idx = pool_index(comp_idx);
            return std::get<std::vector<ComponentType>>(pools_[pool_idx])[entity_index];
        }

        template<typename... CTs>
        [[nodiscard]] std::tuple<CTs&...> get_components(size_t index){
            return std::tie(get_component<CTs>(index)...);
        }

    private:
        [[nodiscard]] size_t pool_index(size_t component_type) const;
        [[nodiscard]] size_t entity_index(const EntityId& entity_id) const;

    protected:
        size_t max_entities_ = 0;
        ComponentMask mask_ = {};
        std::vector<EntityId> entities_ = {};
        std::vector<ComponentPool> pools_ = {};
        std::vector<size_t> component_types_ = {};
    };
}
