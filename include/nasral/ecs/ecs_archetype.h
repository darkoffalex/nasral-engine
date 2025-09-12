#pragma once
#include <nasral/ecs/ecs_types.h>

namespace nasral::ecs
{
    class Archetype
    {
    public:
        typedef std::unique_ptr<Archetype> Ptr;

        struct MoveResult
        {
            /// Индекс нового элемента в архетипе
            size_t new_arch_index = 0;
            /// Если из прежнего архетипа было удаление, на его индекс возьмет элемент с конца списка архетипа
            std::optional<size_t> prev_arch_index = std::nullopt;
            /// Если из прежнего архетипа было удаление, на его место встанет EntityId из конца списка архетипа
            std::optional<EntityId> moved_entity = std::nullopt;
        };

        Archetype(const ComponentMask& mask, size_t max_entities);

        size_t add_entity(const EntityId& id);
        MoveResult move_entity_from(Archetype* from, const EntityId& id);
        MoveResult remove_entity(size_t index);

        [[nodiscard]] const ComponentMask& mask() const { return mask_;}
        [[nodiscard]] const std::vector<EntityId>& entities() const { return entities_;}

        template<typename T>
        [[nodiscard]] bool has_component() const{
            const size_t comp_id = kComponentId<T>;
            return mask_.test(comp_id);
        }

        template<typename T>
        [[nodiscard]] T& get_component(size_t index) {
            const size_t comp_id = kComponentId<T>;
            const size_t pool_idx = get_pool_index(comp_id);
            return std::get<std::vector<T>>(pools_[pool_idx])[index];
        }

        template<typename... Ts>
        [[nodiscard]] std::tuple<Ts&...> get_components(size_t index) {
            return std::tie(get_component<Ts>(index)...);
        }

    private:
        [[nodiscard]] size_t get_pool_index(size_t component_id) const;
        [[nodiscard]] size_t get_entity_index(const EntityId& id) const;

    protected:
        ComponentMask mask_ = {};
        size_t max_entities_ = 0;
        std::vector<EntityId> entities_ = {};
        std::vector<ComponentPoolVariant> pools_ = {};
        std::vector<size_t> component_types_ = {};
    };
}