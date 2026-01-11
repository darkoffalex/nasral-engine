#pragma once

#include <vector>
#include <nasral/core/types.h>
#include <nasral/scn/types.h>
#include <nasral/core/io_struct.h>

namespace nasral::scn::io
{
    struct Node : core::IOStruct
    {
        typedef std::unique_ptr<Node> Ptr;

        struct Data {
            core::UniqueId id = {};
            NodeType type = NodeType::eDummy;
            std::vector<Ptr> children;
        } io_node_data;

        Node(Engine* engine, Data data);
        void unpack_to(const ecs::EntityId& entity_id, UnpackFlags flags) const override;
        void pack_from(ecs::EntityId& entity_id) override;
    };
}