#include "pch.h"
#include <nasral/scn/io/spatial_node.h>

namespace nasral::scn::io
{
    SpatialNode::SpatialNode(Engine *engine, std::tuple<Node::Data, Data> data_tuple)
        : Node(engine, std::move(std::get<0>(data_tuple)))
        , io_spatial_data(std::get<1>(data_tuple))
    {}

    void SpatialNode::unpack_to([[maybe_unused]] const ecs::EntityId &entity_id, [[maybe_unused]] UnpackFlags flags) const
    {
    }

    void SpatialNode::pack_from([[maybe_unused]] ecs::EntityId &entity_id)
    {
    }
}
