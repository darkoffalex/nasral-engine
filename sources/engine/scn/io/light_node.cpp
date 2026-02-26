#include "pch.h"
#include <nasral/scn/io/light_node.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/components.h>
#include <nasral/engine.h>

namespace nasral::scn::io
{
    LightNode::LightNode(Engine *engine, std::tuple<Node::Data, SpatialNode::Data, Data> data_tuple)
        : SpatialNode(engine, {std::move(std::get<0>(data_tuple)), std::get<1>(data_tuple)})
        , io_light_data(std::get<2>(data_tuple))
    {}

    void LightNode::unpack_to([[maybe_unused]] const ecs::EntityId &entity_id, [[maybe_unused]] UnpackFlags flags) const
    {
        try_add_node_components(entity_id);
        try_add_spatial_components(entity_id, false);
        try_add_light_components(entity_id);
    }

    void LightNode::pack_from([[maybe_unused]] ecs::EntityId &entity_id)
    {
    }

    void LightNode::try_add_light_components(const ecs::EntityId& entity_id) const
    {
        auto* ecs = engine()->ecs();
        auto* gfx = engine()->renderer();

        ecs->add_component<comp::Light>(entity_id);
        ecs->add_component<gfx::comp::Activate>(entity_id);

        auto& light = ecs->get_component<comp::Light>(entity_id);
        auto& ubo_id = ecs->get_component<gfx::comp::UniformIndex>(entity_id);
        auto& ubo_s = ecs->get_component<gfx::comp::UniformState>(entity_id);

        light.type = io_light_data.type;
        light.color = io_light_data.color;
        light.intensity = io_light_data.intensity;
        light.radius = io_light_data.radius;
        light.quadratic = io_light_data.quadratic;
        ubo_id.index = gfx->light_ids().acquire();
        ubo_s.dirty = true;
    }
}
