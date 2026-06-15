#include "pch.h"
#include <nasral/scn/objects/spatial.h>
#include <nasral/gfx/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/manager.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Spatial::Spatial(Manager* manager, const NodeDesc& description)
        : Node(manager, description)
    {
        engine()->ecs()->add_components_immediate<Components::Spatial>(entity(),
        {
            description.spatial.position,
            description.spatial.rotation,
            description.spatial.scale
        });
    }

    Spatial::~Spatial() = default;

    data::NodeView Spatial::data_view() const
    {
        const auto [id, name, node, spatial] = engine()->ecs()->get_components<
            Components::Uid,
            Components::Name,
            Components::Node,
            Components::Spatial
        >(entity());

        return data::SpatialNodeView{
            {id.id,name.name,node.type},
            spatial.position,
            spatial.rotation,
            spatial.scale,
        };
    }

    Node::Ptr Spatial::clone() const{
        const auto data = std::get<data::SpatialNodeView>(Spatial::data_view());
        return Node::Ptr{new Spatial(subsystem(), {
            UniqueId::generate(),
            data.type,
            data.name,
            {data.position, data.rotation, data.scale}
        })};
    }

    void Spatial::set_position(const glm::vec3& position) const{
        auto [spatial] = engine()->ecs()->get_components<Components::Spatial>(entity());
        spatial.position = position;
    }

    void Spatial::set_rotation(const glm::vec3& rotation) const{
        auto [spatial] = engine()->ecs()->get_components<Components::Spatial>(entity());
        spatial.rotation = rotation;
    }

    void Spatial::set_scale(const glm::vec3& scale) const{
        auto [spatial] = engine()->ecs()->get_components<Components::Spatial>(entity());
        spatial.scale = scale;
    }
}
