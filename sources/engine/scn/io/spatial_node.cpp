#include "pch.h"
#include <nasral/scn/io/spatial_node.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/components.h>
#include <nasral/engine.h>

namespace nasral::scn::io
{
    SpatialNode::SpatialNode(Engine *engine, std::tuple<Node::Data, Data> data_tuple)
        : Node(engine, std::move(std::get<0>(data_tuple)))
        , io_spatial_data(std::get<1>(data_tuple))
    {}

    void SpatialNode::unpack_to([[maybe_unused]] const ecs::EntityId &entity_id, [[maybe_unused]] UnpackFlags flags) const
    {
        if (!(flags & eUFSkipRootComp)){
            try_add_node_components(entity_id);
        }

        try_add_spatial_components(entity_id);
        try_add_children_components(entity_id, true);
    }

    void SpatialNode::pack_from([[maybe_unused]] ecs::EntityId &entity_id)
    {
        // TODO: Implement
    }

    void SpatialNode::try_add_spatial_components(const ecs::EntityId& entity_id, const bool renderable) const
    {
        auto* ecs = engine()->ecs();
        auto* gfx = engine()->renderer();

        ecs->add_component<comp::Spatial>(entity_id);
        ecs->add_component<gfx::comp::UniformIndex>(entity_id);
        ecs->add_component<gfx::comp::UniformState>(entity_id);

        auto& spatial = ecs->get_component<comp::Spatial>(entity_id);
        spatial.position = io_spatial_data.position;
        spatial.rotation = io_spatial_data.rotation;
        spatial.scale = io_spatial_data.scale;

        if (renderable)
        {
            auto& ubo_id = ecs->get_component<gfx::comp::UniformIndex>(entity_id);
            auto& ubo_s = ecs->get_component<gfx::comp::UniformState>(entity_id);

            ubo_id.index = gfx->object_ids().acquire();
            ubo_s.dirty = true;
        }
    }
}
