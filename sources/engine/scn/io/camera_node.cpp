#include "pch.h"
#include <nasral/scn/io/camera_node.h>

namespace nasral::scn::io
{
    CameraNode::CameraNode(Engine *engine, Data data)
        : SpatialNode(engine, {})
        , io_cam_data(std::move(data))
    {}

    void CameraNode::unpack_to(const ecs::EntityId &entity_id) const
    {
    }

    void CameraNode::pack_from(ecs::EntityId &entity_id)
    {
    }
}
