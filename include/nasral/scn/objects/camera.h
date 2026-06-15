#pragma once

#include <nasral/scn/objects/spatial.h>
#include <nasral/gfx/components.h>

namespace nasral::scn
{
    class Manager;
    class Camera : public Spatial
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<Camera> Ptr;

        struct Components : Spatial::Components
        {
            using View = ViewComponent;
            using UniformIndex = gfx::UniformIndexComponent;
            using UniformState = gfx::UniformStateComponent;
        };

        ~Camera() override;
        Camera(const Camera&) = delete;
        Camera& operator=(const Camera&) = delete;

        [[nodiscard]] data::NodeView data_view() const override;
        [[nodiscard]] Node::Ptr clone() const override;

        void set_position(const glm::vec3& position) const override;
        void set_rotation(const glm::vec3& rotation) const override;
        void set_scale(const glm::vec3& scale) const override;
        void set_type(gfx::ViewType type) const;
        void set_fov(float fov) const;
        void set_aspect(float aspect) const;
        void set_near(float near) const;
        void set_far(float far) const;

    protected:
        Camera(Manager* manager, const NodeDesc& description);
        void invalidate_ubo() const;
    };
}