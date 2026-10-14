#pragma once

#include <nasral/ecs/entity_view.h>
#include <nasral/scn/entities/node.h>
#include <nasral/scn/entities/spatial.h>
#include <nasral/scn/components.h>
#include <nasral/gfx/components.h>
#include <nasral/ecs/components.h>

#define SCN_LIGHT_COMPONENTS \
    scn::LightComponent, \
    gfx::UniformIndexComponent

#define SCN_LIGHT_STATIC_COMPONENTS \
    gfx::DirtyUnformComponent

#define SCN_LIGHT_DYNAMIC_COMPONENTS \
    gfx::UniformStateComponent

namespace nasral::scn
{
    class LightView : public ecs::EntityView<SCN_LIGHT_COMPONENTS>
    {
    public:
        using EntityView::EntityView;

        static LightView setup(ecs::Manager *ecs
            , const ecs::EntityId &id
            , const NodeDesc &desc);

        [[nodiscard]] bool is_dynamic() const;

        [[nodiscard]] gfx::LightType light_type() const;

        [[nodiscard]] const glm::vec4 &light_color() const;

        [[nodiscard]] float light_intensity() const;

        [[nodiscard]] float light_radius() const;

        [[nodiscard]] float light_quadratic() const;

        void set_position(const glm::vec3 &position) const;

        void set_rotation(const glm::vec3 &rotation) const;

        void set_scale(const glm::vec3 &scale) const;

        void set_light_type(gfx::LightType type) const;

        void set_light_color(const glm::vec3 &color) const;

        void set_light_color(const glm::vec4 &color) const;

        void set_light_intensity(float intensity) const;

        void set_light_radius(float radius) const;

        void set_light_quadratic(float quadratic) const;

        void set_active(bool active = true) const;

    protected:
        void invalidate_ubo() const;
    };

    template<>
    struct NodeSpawner<LightView> {
        static std::optional<LightView> spawn(ecs::Manager *ecs, const NodeDesc &desc) {
            const auto entity = ecs->spawn();
            NodeView::setup(ecs, entity, desc);
            SpatialView::setup(ecs, entity, desc);
            return LightView::setup(ecs, entity, desc);
        }
    };
}
