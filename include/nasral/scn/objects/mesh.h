#pragma once

#include <nasral/scn/objects/spatial.h>
#include <nasral/gfx/components.h>
#include <nasral/res/components.h>

namespace nasral::scn
{
    class Manager;
    class Mesh : public Spatial
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<Mesh> Ptr;

        struct Components : Spatial::Components
        {
            using Mesh          = MeshComponent;
            using Resources     = res::ResourcesComponent;
            using Handles       = gfx::MeshHandlesComponent;
            using UniformIndex  = gfx::UniformIndexComponent;
            using UniformState  = gfx::UniformStateComponent;
            using DirtyUniform  = gfx::DirtyUnformComponent;
            using DirtyHandles  = gfx::DirtyHandlesComponent;
            using RenderTag     = gfx::RenderComponent;
        };

        ~Mesh() override;
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        [[nodiscard]] data::NodeView data_view() const override;
        [[nodiscard]] Node::Ptr clone() const override;
        [[nodiscard]] bool is_dynamic() const;
        [[nodiscard]] std::vector<UniqueId> material_uids() const;
        [[nodiscard]] std::string mesh_path() const;

        void set_position(const glm::vec3& position) const override;
        void set_rotation(const glm::vec3& rotation) const override;
        void set_scale(const glm::vec3& scale) const override;
        void set_material(const ecs::EntityId& material, size_t index = 0) const;
        void set_mesh_resource(const res::ResourceId& resource_id) const;

    protected:
        Mesh(Manager* manager, const NodeDesc& description);
        void invalidate_ubo() const;
    };
}