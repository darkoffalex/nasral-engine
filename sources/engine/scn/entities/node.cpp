#include "pch.h"
#include <nasral/scn/entities/node.h>

#include "nasral/engine.h"

namespace nasral::scn
{
    NodeView NodeView::setup(ecs::Manager* ecs
        , const ecs::EntityId& id
        , const NodeDesc& desc)
    {
        ecs->add_components_immediate<SCN_NODE_COMPONENTS>(id,
            {desc.unique_id},
            {desc.name},
            {desc.type});

        return ecs->as_view<NodeView>(id).value();
    }

    const UniqueId& NodeView::uid()
    {
        return get<ecs::UidComponent>().id;
    }

    const std::string& NodeView::name()
    {
        return get<ecs::NameComponent>().name;
    }

    const NodeType& NodeView::type()
    {
        return get<NodeComponent>().type;
    }

    void NodeView::set_uid(const UniqueId& uid)
    {
        get<ecs::UidComponent>().id = uid;
    }

    void NodeView::set_name(const std::string& name)
    {
        get<ecs::NameComponent>().name = name;
    }

    void NodeView::set_type(const NodeType& type)
    {
        get<NodeComponent>().type = type;
    }

    void NodeView::destroy(const bool immediate) const {
        if (immediate) {
            engine()->ecs()->add_components_immediate<ecs::DestroyComponent>(id(), {});
        }else {
            engine()->ecs()->add_components<ecs::DestroyComponent>(id(), {});
        }
    }
}
