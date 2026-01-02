#include "pch.h"
#include <nasral/scn/io/light_node.h>

namespace nasral::scn::io
{
    LightNode::LightNode(Engine *engine, Data data)
        : SpatialNode(engine, {})
        , io_light_data(std::move(data))
    {}

    void LightNode::unpack_to(const ecs::EntityId &entity_id) const
    {
    }

    void LightNode::pack_from(ecs::EntityId &entity_id)
    {
    }
}
