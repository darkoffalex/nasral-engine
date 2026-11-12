#pragma once

#include <nasral/ecs/entity_view.h>
#include <nasral/scn/entities/node.h>
#include <nasral/scn/entities/spatial.h>
#include <nasral/scn/components.h>
#include <nasral/gfx/components.h>

#define SCN_CAMERA_COMPONENTS \
    scn::ViewComponent, \
    gfx::UniformIndexComponent, \
    gfx::UniformStateComponent

namespace nasral::scn {
    class Manager;

    class CameraView : public ecs::EntityView<SCN_CAMERA_COMPONENTS> {
    public:
        friend class Manager;
        using EntityView::EntityView;

        static CameraView setup(ecs::Manager *ecs
                                , const ecs::EntityId &id
                                , const NodeDesc &desc);

        [[nodiscard]] gfx::ViewType type() const;

        [[nodiscard]] float fov() const;

        [[nodiscard]] float aspect() const;

        [[nodiscard]] float near() const;

        [[nodiscard]] float far() const;

        void set_position(const glm::vec3 &position) const;

        void set_rotation(const glm::vec3 &rotation) const;

        void set_scale(const glm::vec3 &scale) const;

        void set_type(gfx::ViewType type) const;

        void set_fov(float fov) const;

        void set_aspect(float aspect) const;

        void set_near(float near) const;

        void set_far(float far) const;

    protected:
        void invalidate_ubo() const;
    };

    template<>
    struct NodeSpawner<CameraView> {
        static std::optional<CameraView> spawn(ecs::Manager *ecs, const NodeDesc &desc) {
            const auto entity = ecs->spawn();
            NodeView::setup(ecs, entity, desc);
            SpatialView::setup(ecs, entity, desc);
            return CameraView::setup(ecs, entity, desc);
        }
    };
}
