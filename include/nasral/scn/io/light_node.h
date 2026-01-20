#pragma once

#include <nasral/scn/io/spatial_node.h>
#include <nasral/gfx/types.h>

namespace nasral::scn::io
{
    struct LightNode : SpatialNode
    {
        struct Data {
            gfx::LightType type = gfx::LightType::ePointLight;
            glm::float32_t intensity = 1.0f;
            glm::float32_t radius = 1.0f;
            glm::float32 quadratic = 0.1f;
            glm::vec4 color = glm::vec4(1.0f);
        } io_light_data;

        typedef std::unique_ptr<Node> Ptr;
        typedef std::tuple<Node::Data, SpatialNode::Data, Data> DataTuple;

        LightNode(Engine* engine, std::tuple<Node::Data, SpatialNode::Data, Data> data_tuple);
        void unpack_to(const ecs::EntityId& entity_id, UnpackFlags flags) const override;
        void pack_from(ecs::EntityId& entity_id) override;

    protected:
        void try_add_light_components(const ecs::EntityId& entity_id) const;
    };
}