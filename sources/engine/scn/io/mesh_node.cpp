#include "pch.h"
#include <nasral/scn/io/mesh_node.h>

namespace nasral::scn::io
{
    MeshNode::MeshNode(Engine *engine, std::tuple<Node::Data, SpatialNode::Data, Data> data_tuple)
        : SpatialNode(engine, {std::move(std::get<0>(data_tuple)), std::get<1>(data_tuple)})
        , io_mesh_data(std::get<2>(data_tuple))
    {}

    void MeshNode::unpack_to([[maybe_unused]] const ecs::EntityId &entity_id, [[maybe_unused]] UnpackFlags flags) const
    {
    }

    void MeshNode::pack_from([[maybe_unused]] ecs::EntityId &entity_id)
    {
    }
}
