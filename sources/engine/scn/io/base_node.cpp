#include "pch.h"
#include <nasral/scn/io/base_node.h>
#include <nasral/ecs/entity.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/components.h>

namespace nasral::scn::io
{
    Node::Node(Engine *engine, Data data)
        : IOStruct(engine)
        , io_node_data(std::move(data))
    {}

    void Node::unpack_to(const ecs::EntityId &entity_id) const
    {
    }

    void Node::pack_from(ecs::EntityId &entity_id)
    {
    }
}
