#pragma once
#include <nasral/ecs/ecs_types.h>

namespace nasral{class Engine;}
namespace nasral::logging{class Logger;}

namespace nasral::ecs
{
    class Archetype
    {
    public:
        typedef std::unique_ptr<Archetype> Ptr;

        struct MoveResult
        {
            std::optional<size_t> new_arch_index = std::nullopt;
            std::optional<size_t> prev_arch_index = std::nullopt;
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

    class EcsManager
    {
    public:
        typedef std::unique_ptr<EcsManager> Ptr;

        struct EntitySlot
        {
            EntityId id = {};
            Archetype* archetype = nullptr;
            ComponentMask mask = {};
            size_t archetype_index = 0;
            bool is_alive = false;
        };

        EcsManager(const Engine* engine, const EcsConfig& config);

        [[nodiscard]] EntityId create_entity();
        void destroy_entity(const EntityId& id);
        void assign_components(const EntityId& id, const ComponentMask& mask);
        void remove_components(const EntityId& id, const ComponentMask& mask);

        template<typename Component>
        void set_component(const EntityId& id, const Component&& component){
            assert(id.index < entities_.size());
            assert(entities_[id.index].archetype != nullptr);

            const auto& slot = entities_[id.index];
            slot.archetype->get_component<Component>(slot.archetype_index) = std::forward<Component>(component);
        }

        template<typename Component>
        void reset_component(const EntityId& id) const{
            assert(id.index < entities_.size());
            assert(entities_[id.index].archetype != nullptr);

            const auto& slot = entities_[id.index];
            slot.archetype->get_component<Component>(slot.archetype_index) = {};
        }

    private:
        [[nodiscard]] const logging::Logger* logger() const;
        [[nodiscard]] Archetype* find_or_create_archetype(const ComponentMask& mask);

    protected:
        SafeHandle<const Engine> engine_;
        EcsConfig config_;
        std::vector<EntitySlot> entities_;
        std::vector<size_t> freed_slots_;
        std::vector<Archetype::Ptr> archetypes_;
    };
}
