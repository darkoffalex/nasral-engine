#include "pch.h"
#include <nasral/scn/entities/camera.h>
#include <nasral/scn/entities/spatial.h>
#include <nasral/engine.h>

namespace nasral::scn {
    CameraView CameraView::setup(ecs::Manager *ecs,
                                 const ecs::EntityId &id,
                                 const NodeDesc &desc) {
        ecs->add_components_immediate<SCN_CAMERA_COMPONENTS>(id,
                                                             {
                                                                 desc.camera.type,
                                                                 desc.camera.fov,
                                                                 desc.camera.aspect,
                                                                 desc.camera.near,
                                                                 desc.camera.far
                                                             },
                                                             {0},
                                                             {true});

        return ecs->as_view<CameraView>(id).value();
    }

    gfx::ViewType CameraView::type() const {
        return get<ViewComponent>().type;
    }

    float CameraView::fov() const {
        return get<ViewComponent>().fov;
    }

    float CameraView::aspect() const {
        return get<ViewComponent>().aspect;
    }

    float CameraView::near() const {
        return get<ViewComponent>().near;
    }

    float CameraView::far() const {
        return get<ViewComponent>().far;
    }

    void CameraView::set_position(const glm::vec3 &position) const {
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()) {
            spatial->set_position(position);
            invalidate_ubo();
        }
    }

    void CameraView::set_rotation(const glm::vec3 &rotation) const {
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()) {
            spatial->set_rotation(rotation);
            invalidate_ubo();
        }
    }

    void CameraView::set_scale(const glm::vec3 &scale) const {
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()) {
            spatial->set_scale(scale);
            invalidate_ubo();
        }
    }

    void CameraView::set_type(const gfx::ViewType type) const {
        auto &view = engine()->ecs()->get_component<ViewComponent>(id());
        view.type = type;
        invalidate_ubo();
    }

    void CameraView::set_fov(const float fov) const {
        auto &view = engine()->ecs()->get_component<ViewComponent>(id());
        view.fov = fov;
        invalidate_ubo();
    }

    void CameraView::set_aspect(const float aspect) const {
        auto &view = engine()->ecs()->get_component<ViewComponent>(id());
        view.aspect = aspect;
        invalidate_ubo();
    }

    void CameraView::set_near(const float near) const {
        auto &view = engine()->ecs()->get_component<ViewComponent>(id());
        view.near = near;
        invalidate_ubo();
    }

    void CameraView::set_far(const float far) const {
        auto &view = engine()->ecs()->get_component<ViewComponent>(id());
        view.far = far;
        invalidate_ubo();
    }

    void CameraView::invalidate_ubo() const {
        auto [ubo_state] = engine()->ecs()->get_components<gfx::UniformStateComponent>(id());
        ubo_state.is_dirty = true;
    }
}
