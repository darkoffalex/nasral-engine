#pragma once
#include <nasral/rendering/rendering_types.h>
#include <nasral/resources/resource_types.h>
#include <nasral/resources/ref.h>

namespace nasral::rendering
{
    class MeshInstance final : public Instance
    {
    public:
        enum ChangesType : uint32_t
        {
            eNone               = 0,
            eMeshChanged        = 1 << 0
        };

        MeshInstance() = default;
        MeshInstance(resources::ResourceManager* manager, const std::string& mesh_path);

        ~MeshInstance() = default;
        MeshInstance(const MeshInstance&) = default;
        MeshInstance& operator=(const MeshInstance&) = default;
        MeshInstance(MeshInstance&& other) noexcept;
        MeshInstance& operator=(MeshInstance&& other) noexcept;

        void set_mesh(const std::string& path, bool request = false);
        void request_resources();
        void release_resources();

        [[nodiscard]] const Handles::Mesh& mesh_render_handles() const;

    private:
        void bind_callbacks();
        void unbind_callbacks();

    protected:
        resources::Ref mesh_ref_;
        Handles::Mesh mesh_handles_ = {};
    };

    static_assert(std::is_move_constructible_v<MeshInstance>, "MeshInstance must be movable-constructible");
    static_assert(std::is_move_assignable_v<MeshInstance>, "MeshInstance must be movable-assignable");
}