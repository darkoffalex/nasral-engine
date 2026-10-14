#pragma once

#include <nasral/ecs/entity_view.h>
#include <nasral/scn/entities/node.h>
#include <nasral/scn/entities/spatial.h>
#include <nasral/scn/components.h>
#include <nasral/res/components.h>
#include <nasral/gfx/components.h>

#define SCN_MESH_COMPONENTS \
    scn::MeshComponent, \
    res::ResourcesComponent, \
    gfx::MeshHandlesComponent, \
    gfx::UniformIndexComponent, \
    gfx::DirtyHandlesComponent, \
    gfx::RenderComponent

#define SCN_MESH_STATIC_COMPONENTS \
    gfx::DirtyUnformComponent

#define SCN_MESH_DYNAMIC_COMPONENTS \
    gfx::UniformStateComponent

namespace nasral::scn
{
    class MeshView : public ecs::EntityView<SCN_MESH_COMPONENTS>
    {
    public:
        using EntityView::EntityView;

        enum class ResIndices : size_t
        {
            eMesh = 0,
        };

        static MeshView setup(ecs::Manager* ecs
            , const ecs::EntityId& id
            , const NodeDesc& desc);

        [[nodiscard]] const ecs::EntityId& material(size_t index = 0) const;
        [[nodiscard]] const res::ResourceId& mesh_resource() const;
        [[nodiscard]] const res::Status& mesh_resource_status() const;
        [[nodiscard]] bool is_dynamic() const;

        void set_position(const glm::vec3& position) const;
        void set_rotation(const glm::vec3& rotation) const;
        void set_scale(const glm::vec3& scale) const;
        void set_material(const ecs::EntityId& material, size_t index = 0) const;
        void set_mesh_resource(const res::ResourceId& resource_id) const;
        void reset_materials() const;
        void reset_mesh_resource() const;

    protected:
        void invalidate_ubo() const;
    };

    template<>
    struct NodeSpawner<MeshView>
    {
        static std::optional<MeshView> spawn(ecs::Manager* ecs, const NodeDesc& desc)
        {
            const auto entity = ecs->spawn();
            NodeView::setup(ecs, entity, desc);
            SpatialView::setup(ecs, entity, desc);
            return MeshView::setup(ecs, entity, desc);
        }
    };
}