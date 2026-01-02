#include "pch.h"
#include <nasral/scn/io/spatial_node.h>

namespace nasral::scn::io
{
    SpatialNode::SpatialNode(Engine *engine, Data data)
        : Node(engine, {})
        , io_spatial_data(std::move(data))
    {}

    void SpatialNode::unpack_to(const ecs::EntityId &entity_id) const
    {
    }

    void SpatialNode::pack_from(ecs::EntityId &entity_id)
    {
    }
}
