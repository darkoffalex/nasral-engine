#include "pch.h"
#include <nasral/ecs/utils.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/components.h>

namespace nasral::ecs
{
    void inc_entity_refs(Manager* m, const EntityId& entity_id, const bool immediate)
    {
        if (!m->has<RefsCountComponent>(entity_id)) return;
        if (!m->is_valid(entity_id)) return;

        m->get_component<RefsCountComponent>(entity_id).count++;

        immediate ?
            m->add_components_immediate<RefsChangedComponent>(entity_id, {}) :
            m->add_components<RefsChangedComponent>(entity_id, {});
    }

    void dec_entity_refs(Manager* m, const EntityId& entity_id, const bool immediate)
    {
        if (!m->has<RefsCountComponent>(entity_id)) return;
        if (!m->is_valid(entity_id)) return;

        m->get_component<RefsCountComponent>(entity_id).count--;

        immediate ?
            m->add_components_immediate<RefsChangedComponent>(entity_id, {}) :
            m->add_components<RefsChangedComponent>(entity_id, {});
    }
}
