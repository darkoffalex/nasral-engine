#pragma once

#include <nasral/scn/io/spatial_node.h>

namespace nasral::scn::io
{
    struct LightNode : SpatialNode
    {
        typedef std::unique_ptr<Node> Ptr;

        struct Data {
            gfx::LightType type = gfx::LightType::ePointLight;
            glm::float32_t intensity = 1.0f;
            glm::float32_t radius = 1.0f;
            glm::float32 quadratic = 0.1f;
            glm::vec4 color = glm::vec4(1.0f);
        } io_light_data;

        LightNode(Engine* engine, Data data);
        void unpack_to(const ecs::EntityId& entity_id) const override;
        void pack_from(ecs::EntityId& entity_id) override;
    };
}