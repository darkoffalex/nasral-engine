#pragma once

#include <nasral/scn/io/spatial_node.h>
#include <nasral/scn/types.h>

namespace nasral::scn::io
{
    struct CameraNode : SpatialNode
    {
        typedef std::unique_ptr<Node> Ptr;

        struct Data {
            CameraType type = CameraType::ePerspective;
            glm::float32_t fov = 90.0f;
            glm::float32_t aspect = 1.0f;
            glm::float32_t near = 0.1f;
            glm::float32_t far = 1000.0f;
        } io_cam_data;

        CameraNode(Engine* engine, Data data);
        void unpack_to(const ecs::EntityId& entity_id) const override;
        void pack_from(ecs::EntityId& entity_id) override;
    };
}