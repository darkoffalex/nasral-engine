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
        template<typename... Ts>
        friend class View;

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
        bool destroy(const EntityId& entity_id);
        void destroy_deferred(const EntityId& entity_id, std::function<void()> on_destroy = nullptr);
        void apply_deferred_actions();

        template <class T>
        void add_component(const EntityId& entity_id) {
            add_component_impl<T>(entity_id, T{});
        }

        template <class T>
        void add_component(const EntityId& entity_id, const T& component) {
            add_component_impl<T>(entity_id, component);
        }

        template <class T>
        void add_component(const EntityId& entity_id, T&& component) {
            add_component_impl<T>(entity_id, std::forward<T>(component));
        }

        template <class T>
        void add_component_deferred(const EntityId& entity_id) {
            add_component_deferred<T>(entity_id, T{});
        }

        template <class T>
        void add_component_deferred(const EntityId& entity_id, const T& component) {
            using U = std::decay_t<T>;
            deferred_actions_.emplace_back(
                [entity_id, comp = U(component)](Manager& m) mutable {
                    m.add_component<U>(entity_id, std::move(comp));
                }
            );
        }

        template <class T>
        void add_component_deferred(const EntityId& entity_id, T&& component) {
            using U = std::decay_t<T>;
            deferred_actions_.emplace_back(
                [entity_id, comp = U(std::forward<T>(component))](Manager& m) mutable {
                    m.add_component<U>(entity_id, std::move(comp));
                }
            );
        }

        template<typename CT>
        void remove_component(const EntityId& entity_id){
            auto& slot = entities_[entity_id.index];
            slot.mask.reset(kComponentId<CT>);
            auto* archetype = find_or_create_archetype(slot.mask);
            assign_archetype(slot, archetype);
        }

        template<typename CT>
        void remove_component_deferred(const EntityId& entity_id){
            deferred_actions_.emplace_back([entity_id](Manager& m){
                m.remove_component<CT>(entity_id);
            });
        }

        template<typename CT>
        [[nodiscard]] bool has_component(const EntityId& entity_id) const{
            const auto& slot = entities_[entity_id.index];
            return slot.mask.test(kComponentId<CT>);
        }

        template<typename CT>
        CT& get_component(const EntityId& entity_id){
            const auto& slot = entities_[entity_id.index];
            return slot.archetype->get_component<CT>(slot.index_in_archetype);
        }

        template<typename CT>
        CT& get_or_add_component(const EntityId& entity_id){
            if (has_component<CT>(entity_id)){ return get_component<CT>(entity_id); }
            add_component<CT>(entity_id);
            return get_component<CT>(entity_id);
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
        template <class T, class V>
        void add_component_impl(const EntityId& entity_id, V&& value){
            auto& slot = entities_[entity_id.index];
            slot.mask.set(kComponentId<T>);
            auto* archetype = find_or_create_archetype(slot.mask);
            assign_archetype(slot, archetype);
            archetype->get_component<T>(slot.index_in_archetype) = std::forward<V>(value);
        }

        [[nodiscard]] Archetype* find_or_create_archetype(const ComponentMask& mask);
        void assign_archetype(EntitySlot& slot, Archetype* archetype);

    protected:
        typedef std::function<void(Manager&)> DeferredAction;

        std::vector<EntitySlot> entities_;
        std::vector<size_t> free_slots_;
        std::vector<Archetype::Ptr> archetypes_;
        std::vector<DeferredAction> deferred_actions_;
    };
}