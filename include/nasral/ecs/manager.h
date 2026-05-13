#pragma once
#include <nasral/common/subsystem.h>
#include <nasral/ecs/archetype.h>
#include <nasral/log/loggable.h>

namespace nasral::ecs
{
    template<typename... CTs>
    class View;

    class Manager final : public Subsystem<Manager, Config>, public log::Loggable<Manager>
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
            size_t index_in_arch = 0;
        };

        explicit Manager(Engine* e, const Config& config);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void init();
        void finalize();

        [[nodiscard]] EntityId spawn();

        void destroy(const EntityId& entity);
        void destroy_immediate(const EntityId& entity);

        template<typename... CTs>
        void add_components(const EntityId& entity, CTs&&... components){
            using ComponentsTuple = std::tuple<std::decay_t<CTs>...>;
            defer([
                entity,
                comps = ComponentsTuple(std::forward<CTs>(components)...)
            ](Manager& m) mutable {
                std::apply(
                    [&m, &entity](auto&&... unpacked){
                        m.add_components_impl(entity, std::forward<decltype(unpacked)>(unpacked)...);
                    },
                    std::move(comps)
                );
            });
        }

        template<typename... CTs>
        void add_components_immediate(const EntityId& entity, CTs&&... components){
            add_components_impl(entity, std::forward<CTs>(components)...);
        }

        template<typename... CTs>
        void remove_components(const EntityId& entity){
            defer([entity](Manager& m){
                m.remove_components_impl<CTs...>(entity);
            });
        }

        template<typename... CTs>
        void remove_components_immediate(const EntityId& entity){
            remove_components_impl<CTs...>(entity);
        }

        template<typename... CTs>
        [[nodiscard]] bool has_components(const EntityId& entity) const noexcept{
            return entities_[entity.index].mask.test(kComponentId<std::decay_t<CTs>>...);
        }

        template<typename CT>
        CT& get_component(const EntityId& entity) const noexcept{
            const auto& slot = entities_[entity.index];
            return slot.archetype->component<CT>(slot.index_in_arch);
        }

        template<typename... CTs>
        std::tuple<CTs&...> get_components(const EntityId& entity) const noexcept{
            const auto& slot = entities_[entity.index];
            return slot.archetype->components<CTs...>(slot.index_in_arch);
        }

        [[nodiscard]] bool is_valid(const EntityId& entity) const noexcept{
            if (entity.index >= entities_.size()){ return false;}
            return entities_[entity.index].archetype != nullptr
                && entities_[entity.index].id == entity;
        }

        template<typename... CTs>
        View<CTs...> view(const ComponentMask& exclusion = {}){
            return View<CTs...>(this, exclusion);
        }

    private:
        template<typename... CTs>
        void add_components_impl(const EntityId& entity, CTs&&... components){
            auto& slot = entities_[entity.index];
            (slot.mask.set(kComponentId<std::decay_t<CTs>>), ...);
            auto* archetype = ensure_archetype(slot.mask);
            assign_archetype(slot, archetype);
            ((archetype->component<std::decay_t<CTs>>(slot.index_in_arch) = std::forward<CTs>(components)), ...);
        }

        template<typename... CTs>
        void remove_components_impl(const EntityId& entity){
            auto& slot = entities_[entity.index];
            (slot.mask.reset(kComponentId<std::decay_t<CTs>>), ...);
            auto* archetype = ensure_archetype(slot.mask);
            assign_archetype(slot, archetype);
        }

        Archetype* ensure_archetype(const ComponentMask& mask);
        void assign_archetype(EntitySlot& slot, Archetype* archetype);

    protected:
        std::vector<EntitySlot> entities_;
        std::vector<size_t> free_slots_;
        std::vector<Archetype::Ptr> archetypes_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(ecs::Manager)
