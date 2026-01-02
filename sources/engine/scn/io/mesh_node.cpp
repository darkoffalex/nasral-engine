#include "pch.h"
#include <nasral/scn/io/mesh_node.h>

namespace nasral::scn::io
{
    MeshNode::MeshNode(Engine *engine, Data data)
        : SpatialNode(engine, {})
        , io_mesh_data(std::move(data))
    {}

    void MeshNode::unpack_to(const ecs::EntityId &entity_id) const
    {
    }

    void MeshNode::pack_from(ecs::EntityId &entity_id)
    {
    }
}
