#include "pch.h"
#include <nasral/scn/entities/light.h>
#include <nasral/scn/entities/spatial.h>
#include <nasral/gfx/manager.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    LightView LightView::setup(ecs::Manager *ecs
        , const ecs::EntityId &id
        , const NodeDesc &desc)
    {
        auto *gfx = ecs->engine()->gfx();

        LightComponent light_c{
            desc.light.type,
            desc.light.intensity,
            desc.light.radius,
            desc.light.quadratic,
            desc.light.color
        };

        gfx::UniformIndexComponent ubo_c{gfx->light_ubo_ids().acquire()};

        if (desc.light.dynamic) {
            if (desc.light.is_active) {
                ecs->add_components_immediate(id,
                    light_c,
                    ubo_c,
                    gfx::UniformStateComponent{true},
                    ecs::ActivateComponent{}
                );
            } else {
                ecs->add_components_immediate(id,
                    light_c,
                    ubo_c,
                    gfx::UniformStateComponent{true}
                );
            }
        } else {
            if (desc.light.is_active) {
                ecs->add_components_immediate(id,
                    light_c,
                    ubo_c,
                    gfx::DirtyUnformComponent{},
                    ecs::ActivateComponent{}
                );
            } else {
                ecs->add_components_immediate(id,
                    light_c,
                    ubo_c,
                    gfx::DirtyUnformComponent{}
                );
            }
        }

        return ecs->as_view<LightView>(id).value();
    }

    bool LightView::is_dynamic() const {
        return subsystem()->has<gfx::UniformStateComponent>(id());
    }

    gfx::LightType LightView::light_type() const {
        return get<LightComponent>().type;
    }

    const glm::vec4 &LightView::light_color() const {
        return get<LightComponent>().color;
    }

    float LightView::light_intensity() const {
        return get<LightComponent>().intensity;
    }

    float LightView::light_radius() const {
        return get<LightComponent>().radius;
    }

    float LightView::light_quadratic() const {
        return get<LightComponent>().quadratic;
    }

    void LightView::set_position(const glm::vec3 &position) const {
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()) {
            spatial->set_position(position);
            invalidate_ubo();
        }
    }

    void LightView::set_rotation(const glm::vec3 &rotation) const {
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()) {
            spatial->set_rotation(rotation);
            invalidate_ubo();
        }
    }

    void LightView::set_scale(const glm::vec3 &scale) const {
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()) {
            spatial->set_scale(scale);
            invalidate_ubo();
        }
    }

    void LightView::set_light_type(const gfx::LightType type) const {
        auto &settings = engine()->ecs()->get_component<LightComponent>(id());
        settings.type = type;
        invalidate_ubo();
    }

    void LightView::set_light_color(const glm::vec3 &color) const {
        auto &settings = engine()->ecs()->get_component<LightComponent>(id());
        settings.color = {color.x, color.y, color.z, 1.0f};
        invalidate_ubo();
    }

    void LightView::set_light_color(const glm::vec4 &color) const {
        auto &settings = engine()->ecs()->get_component<LightComponent>(id());
        settings.color = color;
        invalidate_ubo();
    }

    void LightView::set_light_intensity(const float intensity) const {
        auto &settings = engine()->ecs()->get_component<LightComponent>(id());
        settings.intensity = intensity;
        invalidate_ubo();
    }

    void LightView::set_light_radius(const float radius) const {
        auto &settings = engine()->ecs()->get_component<LightComponent>(id());
        settings.radius = radius;
        invalidate_ubo();
    }

    void LightView::set_light_quadratic(const float quadratic) const {
        auto &settings = engine()->ecs()->get_component<LightComponent>(id());
        settings.quadratic = quadratic;
        invalidate_ubo();
    }

    void LightView::set_active(const bool active) const {
        if (active) {
            engine()->ecs()->add_components<ecs::ActivateComponent>(id(), {});
        } else {
            engine()->ecs()->add_components<ecs::DeactivateComponent>(id(), {});
        }
    }

    void LightView::invalidate_ubo() const {
        if (is_dynamic()) {
            auto [ubo_state] = engine()->ecs()->get_components<gfx::UniformStateComponent>(id());
            ubo_state.is_dirty = true;
        } else if (!engine()->ecs()->has<gfx::DirtyUnformComponent>(id())) {
            engine()->ecs()->add_components<gfx::DirtyUnformComponent>(id(), {});
        }
    }
}
