#pragma once

#include <nasral/core/subsystem.h>
#include <nasral/ecs/archetype.h>

namespace nasral::ecs
{
    template<typename... CTs>
    class View;

    class Manager final : public core::Subsystem<Config>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;
        friend class View<>;

        struct EntitySlot
        {
            EntityId id = {};
            Archetype* archetype = nullptr;
            ComponentMask mask = {};
            size_t index_in_archetype = 0;
            bool is_alive = true;
        };

        Manager(Engine* engine, const Config& config);

        [[nodiscard]] EntityId spawn();
        void destroy(const EntityId& entity_id);

        template<typename CT>
        void add_component(const EntityId& entity_id, CT&& component = {}){
            auto& slot = entities_[entity_id.index];
            slot.mask.set(kComponentId<CT>);
            auto* archetype = find_or_create_archetype(slot.mask);
            assign_archetype(slot, archetype);
            archetype->get_component<CT>(slot.index_in_archetype) = std::forward<CT>(component);
        }

        template<typename CT>
        void remove_component(const EntityId& entity_id){
            auto& slot = entities_[entity_id.index];
            slot.mask.reset(kComponentId<CT>);
            auto* archetype = find_or_create_archetype(slot.mask);
            assign_archetype(slot, archetype);
        }

        template<typename CT>
        CT& get_component(const EntityId& entity_id){
            const auto& slot = entities_[entity_id.index];
            if (!slot.archetype->has_component<CT>()){
                throw std::runtime_error("Entity does not have component");
            }
            return slot.archetype->get_component<CT>(slot.index_in_archetype);
        }

        template<typename... CTs>
        std::tuple<CTs&...> get_components(const EntityId& entity_id){
            return std::tie(get_component<CTs>(entity_id)...);
        }

        [[nodiscard]] bool is_alive(const EntityId& entity_id) const noexcept{
            if (entity_id.index >= entities_.size()){ return false;}
            return entities_[entity_id.index].is_alive;
        }

        [[nodiscard]] bool is_valid(const EntityId& entity_id) const noexcept{
            if (entity_id.index >= entities_.size()){ return false;}
            return entities_[entity_id.index].is_alive
                && entities_[entity_id.index].archetype != nullptr
                && entities_[entity_id.index].id == entity_id;
        }

        template<typename... CTs>
        View<CTs...> view();

    private:
        [[nodiscard]] Archetype* find_or_create_archetype(const ComponentMask& mask);
        void assign_archetype(EntitySlot& slot, Archetype* archetype);

    protected:
        std::vector<EntitySlot> entities_;
        std::vector<size_t> free_slots_;
        std::vector<Archetype::Ptr> archetypes_;
    };
}