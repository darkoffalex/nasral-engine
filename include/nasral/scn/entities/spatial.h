#pragma once

#include <nasral/ecs/entity_view.h>
#include <nasral/scn/components.h>
#include <nasral/scn/entities/node.h>

#define SCN_SPATIAL_COMPONENTS \
    scn::SpatialComponent

namespace nasral::scn
{
    class SpatialView : public ecs::EntityView<SCN_SPATIAL_COMPONENTS>
    {
    public:
        using EntityView::EntityView;

        static SpatialView setup(ecs::Manager* ecs
            , const ecs::EntityId& id
            , const NodeDesc& desc);

        [[nodiscard]] const glm::vec3& position();
        [[nodiscard]] const glm::vec3& rotation();
        [[nodiscard]] const glm::vec3& scale();

        void set_position(const glm::vec3& position);
        void set_rotation(const glm::vec3& rotation);
        void set_scale(const glm::vec3& scale);
    };

    template<>
    struct NodeSpawner<SpatialView>
    {
        static std::optional<SpatialView> spawn(ecs::Manager* ecs, const NodeDesc& desc)
        {
            const auto entity = ecs->spawn();
            NodeView::setup(ecs, entity, desc);
            return SpatialView::setup(ecs, entity, desc);
        }
    };
}