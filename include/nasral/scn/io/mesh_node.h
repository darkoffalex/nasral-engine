#pragma once

#include <nasral/scn/io/spatial_node.h>

namespace nasral::scn::io
{
    struct MeshNode : SpatialNode
    {
        typedef std::unique_ptr<Node> Ptr;

        struct Data {
            std::string mesh_path;
            core::UniqueId material_id = {};
        } io_mesh_data;

        MeshNode(Engine* engine, Data data);
        void unpack_to(const ecs::EntityId& entity_id) const override;
        void pack_from(ecs::EntityId& entity_id) override;
    };
}