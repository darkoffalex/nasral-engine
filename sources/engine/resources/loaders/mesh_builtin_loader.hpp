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

                std::vector<rendering::Vertex> vertices;
                std::vector<uint32_t> indices;

                // Грань +Z (передняя, как в вашем квадрате)
                vertices.insert(vertices.end(), {
                    {{-half_size, -half_size, half_size}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, half_size}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, half_size}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{half_size, -half_size, half_size}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                uint32_t base = vertices.size() - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань -Z (задняя)
                vertices.insert(vertices.end(), {
                    {{half_size, -half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{-half_size, -half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = vertices.size() - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань +X (правая)
                vertices.insert(vertices.end(), {
                    {{half_size, -half_size, half_size}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, half_size}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, -half_size}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{half_size, -half_size, -half_size}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = vertices.size() - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань -X (левая)
                vertices.insert(vertices.end(), {
                    {{-half_size, -half_size, -half_size}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, -half_size}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, half_size}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{-half_size, -half_size, half_size}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = vertices.size() - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань +Y (верхняя)
                vertices.insert(vertices.end(), {
                    {{-half_size, half_size, half_size}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, -half_size}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, -half_size}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{half_size, half_size, half_size}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = vertices.size() - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань -Y (нижняя)
                vertices.insert(vertices.end(), {
                    {{-half_size, -half_size, -half_size}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size, -half_size, half_size}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{half_size, -half_size, half_size}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{half_size, -half_size, -half_size}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = vertices.size() - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                return std::optional{Mesh::Data{
                    std::move(vertices),
                    std::move(indices)
                }};
            }

            return std::nullopt;
        }
    };
}