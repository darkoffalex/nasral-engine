#pragma once
#include <nasral/ecs/ecs_types.h>

namespace nasral::ecs
{
    class ECSManager
    {
    public:
        typedef std::unique_ptr<ECSManager> Ptr;

        ECSManager();
        ECSManager(const ECSManager&) = delete;
        ECSManager& operator=(const ECSManager&) = delete;

        EntityId create_entity_unsafe();
        EntityId create_entity();
        void destroy_entity_unsafe(const EntityId& id);
        void destroy_entity(const EntityId& id);

        template<class Component>
        uint32_t component_id()
        {
            static uint32_t id = component_counter_.fetch_add(1);
            return id;
        }

        template<class Component>
        Component* assign_component(const EntityId& id){
            assert(id.index < MAX_ENTITIES);
            auto cmp_id = component_id<Component>();
            if (entities_[id.index].deleted.load()) return nullptr;
            if (entities_[id.index].id.version != id.version) return nullptr;
            if (entities_[id.index].used_components.get(cmp_id)) return nullptr;

            entities_[id.index].used_components.set(cmp_id, true);
            if (component_pools_[cmp_id].get() == nullptr){
                component_pools_[cmp_id] = std::make_unique<ComponentPool<Component>>(MAX_ENTITIES);
            }

            return component_pools_[cmp_id]->component(id.index);
        }

        template<class Component>
        void remove_component(const EntityId& id){
            assert(id.index < MAX_ENTITIES);
            auto cmp_id = component_id<Component>();
            if (entities_[id.index].deleted.load()) return;
            if (entities_[id.index].id.version != id.version) return;
            entities_[id.index].used_components.set(cmp_id, false);
        }

        template<class Component>
        [[nodiscard]] Component* component_of(const EntityId& id){
            auto cmp_id = component_id<Component>();
            if (entities_[id.index].deleted.load()) return nullptr;
            if (entities_[id.index].id.version != id.version) return nullptr;
            if (!entities_[id.index].used_components.get(cmp_id)) return nullptr;
            return static_cast<ComponentPool<Component>*>(component_pools_[cmp_id].get())->component(id.index);
        }

    private:
        std::atomic_uint32_t component_counter_{0};
        std::atomic_size_t entity_counter_{0};
        std::vector<size_t> freed_entities_;
        std::mutex freed_entities_mutex_;
        std::array<EntitySlot, MAX_ENTITIES> entities_;
        std::array<ComponentPoolBase::Ptr, MAX_UNIQUE_COMPONENTS> component_pools_;
    };
}
