#pragma once

#include <nasral/scn/io/spatial_node.h>

namespace nasral::scn::io
{
    struct MeshNode : SpatialNode
    {
        struct Data {
            std::string mesh_path;
            core::UniqueId material_id = {};
        } io_mesh_data;

        typedef std::unique_ptr<Node> Ptr;
        typedef std::tuple<Node::Data, SpatialNode::Data, Data> DataTuple;

        MeshNode(Engine* engine, DataTuple data_tuple);
        void unpack_to(const ecs::EntityId& entity_id, UnpackFlags flags) const override;
        void pack_from(ecs::EntityId& entity_id) override;
    };
}