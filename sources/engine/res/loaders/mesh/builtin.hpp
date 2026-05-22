#pragma once

#include <nasral/res/objects/mesh.h>
#include <nasral/gfx/utils.h>

namespace nasral::res
{
    class MeshBuiltinLoader final : public Mesh::Loader<Mesh::Data>
    {
    public:
        explicit MeshBuiltinLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<Mesh::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            std::vector<gfx::Vertex> vertices = {};
            std::vector<uint32_t> indices = {};

            if (path.find(kBuiltinMeshQuad) != std::string::npos){
                auto [v, i] = gfx::gen_quad_geometry(1.0f);
                vertices = std::move(v);
                indices = std::move(i);
            }
            else if (path.find(kBuiltinMeshCube) != std::string::npos){
                auto [v, i] = gfx::gen_cube_geometry(1.0f);
                vertices = std::move(v);
                indices = std::move(i);
            }
            else if (path.find(kBuiltinMeshSphere) != std::string::npos){
                auto* lp = params<MeshLoadParams>();
                auto [v, i] = gfx::gen_sphere_geometry(0.5f, 32, 16, !lp || !lp->winding_order_ccw);
                vertices = std::move(v);
                indices = std::move(i);
            }

            if (vertices.empty() || indices.empty()){
                set_error(Error::eUnknownResource);
                return std::nullopt;
            }

            return std::optional{Mesh::Data{
                std::move(vertices),
                std::move(indices),
                {{0, static_cast<uint32_t>(indices.size()), 0}}
            }};
        }
    };
}