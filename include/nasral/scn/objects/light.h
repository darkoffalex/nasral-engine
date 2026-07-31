#pragma once

#include <nasral/scn/objects/spatial.h>
#include <nasral/gfx/components.h>
#include <nasral/res/components.h>

namespace nasral::scn
{
    class Manager;
    class Light : public Spatial
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<Light> Ptr;

        struct Components : Spatial::Components
        {
            using Light = LightComponent;
            using UniformIndex = gfx::UniformIndexComponent;
            using UniformState = gfx::UniformStateComponent;
            using DirtyUniform = gfx::DirtyUnformComponent;
            using Activate = ecs::ActivateComponent;
            using Deactivate = ecs::DeactivateComponent;
        };

        ~Light() override;
        Light(const Light&) = delete;
        Light& operator=(const Light&) = delete;

        [[nodiscard]] data::NodeView data_view() const override;
        [[nodiscard]] Node::Ptr clone() const override;
        [[nodiscard]] bool is_dynamic() const;

        void set_position(const glm::vec3& position) const override;
        void set_rotation(const glm::vec3& rotation) const override;
        void set_scale(const glm::vec3& scale) const override;
        void set_light_type(gfx::LightType type) const;
        void set_light_color(const glm::vec3& color) const;
        void set_light_intensity(float intensity) const;
        void set_light_radius(float radius) const;
        void set_light_quadratic(float quadratic) const;

    protected:
        Light(Manager* manager, const NodeDesc& description);
        void invalidate_ubo() const;
    };
}