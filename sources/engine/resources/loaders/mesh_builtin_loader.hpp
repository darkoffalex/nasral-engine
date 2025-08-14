#pragma once
#include <nasral/resources/mesh.h>

namespace nasral::resources
{
    class MeshBuiltinLoader final : public Loader<Mesh::Data>
    {
    public:
        std::optional<Mesh::Data> load([[maybe_unused]] const std::string_view& path) override{
            const auto quad_name = name_of(BuiltinResources::eQuadMesh, kBuiltinResources);
            const auto cube_name = name_of(BuiltinResources::eCubeMesh, kBuiltinResources);

            if (path.find(quad_name) != std::string_view::npos){
                constexpr float size = 1.0f;
                constexpr float half_size = size / 2.0f;
                std::vector<rendering::Vertex> vertices = {
                    {{-half_size, -half_size, 0.0f},{0.0f, 0.0f, 1.0f},{0.0f, 0.0f},{1.0f, 0.0f,0.0f,1.0f}},
                    {{-half_size, half_size, 0.0f},{0.0f, 0.0f, 1.0f},{0.0f, 1.0f},{0.0f, 1.0f,0.0f,1.0f}},
                    {{half_size, half_size, 0.0f},{0.0f, 0.0f, 1.0f},{1.0f, 1.0f},{0.0f, 0.0f,1.0f,1.0f}},
                    {{half_size, -half_size, 0.0f},{0.0f, 0.0f, 1.0f},{1.0f, 0.0f},{1.0f, 1.0f,0.0f,1.0f}}
                };
                std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
                return std::optional{Mesh::Data{
                    std::move(vertices),
                    std::move(indices)
                }};
            }
            if (path.find(cube_name) != std::string_view::npos){
                constexpr float size = 1.0f;
                constexpr float half_size = size / 2.0f;
                std::vector<rendering::Vertex> vertices = {
                    {{-half_size, -half_size,  half_size}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size,  half_size,  half_size}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{ half_size,  half_size,  half_size}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{ half_size, -half_size,  half_size}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

                    {{-half_size, -half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size,  half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{ half_size,  half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{ half_size, -half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

                    {{ half_size, -half_size, -half_size}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{ half_size,  half_size, -half_size}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{ half_size,  half_size,  half_size}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{ half_size, -half_size,  half_size}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

                    {{-half_size, -half_size, -half_size}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size,  half_size, -half_size}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{-half_size,  half_size,  half_size}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{-half_size, -half_size,  half_size}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

                    {{-half_size,  half_size, -half_size}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size,  half_size,  half_size}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{ half_size,  half_size,  half_size}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{ half_size,  half_size, -half_size}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

                    {{-half_size, -half_size, -half_size}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size, -half_size,  half_size}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{ half_size, -half_size,  half_size}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{ half_size, -half_size, -half_size}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                };

                std::vector<uint32_t> indices = {
                    0, 1, 2,  0, 2, 3,
                    4, 6, 5,  4, 7, 6,
                    8, 9, 10, 8, 10, 11,
                    12, 14, 13, 12, 15, 14,
                    16, 17, 18, 16, 18, 19,
                    20, 22, 21, 20, 23, 22
                };

                return std::optional{Mesh::Data{
                    std::move(vertices),
                    std::move(indices)
                }};
            }

            return std::nullopt;
        }
    };
}