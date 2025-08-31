#include "pch.h"
#include <nasral/ecs/ecs_manager.h>

namespace nasral::ecs
{
    ECSManager::ECSManager(){
        freed_entities_.reserve(MAX_ENTITIES);
    }

    EntityId ECSManager::create_entity_unsafe(){
        if (!freed_entities_.empty()){
            const auto idx = freed_entities_.back();
            freed_entities_.pop_back();

            entities_[idx].deleted.store(false, std::memory_order_release);
            return entities_[idx].id;
        }

        const auto last_idx = entity_counter_.fetch_add(1);
        if (last_idx >= MAX_ENTITIES){
            entity_counter_.fetch_sub(1);
            throw ECSError("Too many entities");
        }

        auto& entity = entities_[last_idx];
        entity.id.index = last_idx;
        entity.deleted.store(false, std::memory_order_release);
        return entity.id;
    }

    EntityId ECSManager::create_entity(){
        std::lock_guard lock(freed_entities_mutex_);
        return create_entity_unsafe();
    }

    void ECSManager::destroy_entity_unsafe(const EntityId& id){
        assert(id.index < MAX_ENTITIES);
        entities_[id.index].deleted.store(true, std::memory_order_release);
        entities_[id.index].id.version++;
        entities_[id.index].used_components.reset();
        freed_entities_.push_back(id.index);
    }

    void ECSManager::destroy_entity(const EntityId& id){
        std::lock_guard lock(freed_entities_mutex_);
        destroy_entity_unsafe(id);
    }
}
