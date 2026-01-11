#pragma once
#include <nasral/scn/io/base_node.h>

namespace nasral::scn::io
{
    struct SpatialNode : Node
    {
        typedef std::unique_ptr<Node> Ptr;

        struct Data {
            glm::vec3 position = {0.0f, 0.0f, 0.0f};
            glm::vec3 scale = {1.0f, 1.0f, 1.0f};
            glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
        } io_spatial_data;

        SpatialNode(Engine* engine, std::tuple<Node::Data, Data> data_tuple);
        void unpack_to(const ecs::EntityId& entity_id, UnpackFlags flags) const override;
        void pack_from(ecs::EntityId& entity_id) override;
    };
}