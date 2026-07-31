#include "pch.h"
#include <nasral/scn/objects/camera.h>
#include <nasral/gfx/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/manager.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Camera::Camera(Manager* manager, const NodeDesc& description)
        : Spatial(manager, description)
    {
        engine()->ecs()->add_components_immediate<
            Components::View,
            Components::UniformIndex,
            Components::UniformState>(entity(),
            {
                description.camera.type,
                description.camera.fov,
                description.camera.aspect,
                description.camera.near,
                description.camera.far,
            },
            {0},
            {true});
    }

    Camera::~Camera() = default;

    data::NodeView Camera::data_view() const
    {
        const auto [id, name, node, spatial, view] = engine()->ecs()->get_components<
                Components::Uid,
                Components::Name,
                Components::Node,
                Components::Spatial,
                Components::View
            >(entity());

        return data::CameraNodeView{
            {
                {id.id,name.name,node.type},
                spatial.position,
                spatial.rotation,
                spatial.scale,
            },
            view.type,
            view.fov,
            view.aspect,
            view.near,
            view.far,
        };
    }

    Node::Ptr Camera::clone() const{
        const auto data = std::get<data::CameraNodeView>(Spatial::data_view());
        return Node::Ptr{new Camera(subsystem(), {
            UniqueId::generate(),
            data.type,
            data.name,
            {data.position, data.rotation, data.scale},
            {data.view_type, data.fov, data.aspect, data.near, data.far},
        })};
    }

    void Camera::set_position(const glm::vec3& position) const{
        Spatial::set_position(position);
        invalidate_ubo();
    }

    void Camera::set_rotation(const glm::vec3& rotation) const{
        Spatial::set_rotation(rotation);
        invalidate_ubo();
    }

    void Camera::set_scale(const glm::vec3& scale) const{
        Spatial::set_scale(scale);
        invalidate_ubo();
    }

    void Camera::set_type(const gfx::ViewType type) const
    {
        auto [view, ubo_state] = engine()->ecs()->get_components<
            Components::View,
            Components::UniformState>(entity());

        view.type = type;
        ubo_state.is_dirty = true;
    }

    void Camera::set_fov(const float fov) const
    {
        auto [view, ubo_state] = engine()->ecs()->get_components<
            Components::View,
            Components::UniformState>(entity());

        view.fov = fov;
        ubo_state.is_dirty = true;
    }

    void Camera::set_aspect(const float aspect) const
    {
        auto [view, ubo_state] = engine()->ecs()->get_components<
            Components::View,
            Components::UniformState>(entity());

        view.aspect = aspect;
        ubo_state.is_dirty = true;
    }

    void Camera::set_near(const float near) const
    {
        auto [view, ubo_state] = engine()->ecs()->get_components<
            Components::View,
            Components::UniformState>(entity());

        view.near = near;
        ubo_state.is_dirty = true;
    }

    void Camera::set_far(const float far) const
    {
        auto [view, ubo_state] = engine()->ecs()->get_components<
            Components::View,
            Components::UniformState>(entity());

        view.far = far;
        ubo_state.is_dirty = true;
    }

    void Camera::invalidate_ubo() const
    {
        auto [ubo_state] = engine()->ecs()->get_components<Components::UniformState>(entity());
        ubo_state.is_dirty = true;
    }
}
