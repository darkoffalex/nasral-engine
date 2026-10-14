#include "pch.h"
#include <nasral/scn/entities/spatial.h>

namespace nasral::scn
{
    SpatialView SpatialView::setup(ecs::Manager* ecs,
        const ecs::EntityId& id,
        const NodeDesc& desc)
    {
        ecs->add_components_immediate<SCN_SPATIAL_COMPONENTS>(id,
            {
                desc.spatial.position,
                desc.spatial.rotation,
                desc.spatial.scale
            });

        return ecs->as_view<SpatialView>(id).value();
    }

    const glm::vec3& SpatialView::position()
    {
        return get<SpatialComponent>().position;
    }

    const glm::vec3& SpatialView::rotation()
    {
        return get<SpatialComponent>().rotation;
    }

    const glm::vec3& SpatialView::scale()
    {
        return get<SpatialComponent>().scale;
    }

    void SpatialView::set_position(const glm::vec3& position)
    {
        get<SpatialComponent>().position = position;
    }

    void SpatialView::set_rotation(const glm::vec3& rotation)
    {
        get<SpatialComponent>().rotation = rotation;
    }

    void SpatialView::set_scale(const glm::vec3& scale)
    {
        get<SpatialComponent>().scale = scale;
    }
}
