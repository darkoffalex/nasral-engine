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
            const auto sphere_name = name_of(BuiltinResources::eSphereMesh, kBuiltinResources);

            // Квадрат
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

            // Куб
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
                uint32_t base = static_cast<uint32_t>(vertices.size()) - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань -Z (задняя)
                vertices.insert(vertices.end(), {
                    {{half_size, -half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{-half_size, -half_size, -half_size}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = static_cast<uint32_t>(vertices.size()) - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань +X (правая)
                vertices.insert(vertices.end(), {
                    {{half_size, -half_size, half_size}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, half_size}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, -half_size}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{half_size, -half_size, -half_size}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = static_cast<uint32_t>(vertices.size()) - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань -X (левая)
                vertices.insert(vertices.end(), {
                    {{-half_size, -half_size, -half_size}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, -half_size}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, half_size}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{-half_size, -half_size, half_size}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = static_cast<uint32_t>(vertices.size()) - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань +Y (верхняя)
                vertices.insert(vertices.end(), {
                    {{-half_size, half_size, half_size}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size, half_size, -half_size}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{half_size, half_size, -half_size}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{half_size, half_size, half_size}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = static_cast<uint32_t>(vertices.size()) - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                // Грань -Y (нижняя)
                vertices.insert(vertices.end(), {
                    {{-half_size, -half_size, -half_size}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                    {{-half_size, -half_size, half_size}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                    {{half_size, -half_size, half_size}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                    {{half_size, -half_size, -half_size}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}
                });
                base = static_cast<uint32_t>(vertices.size()) - 4;
                indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});

                err_code_ = ErrorCode::eNoError;
                return std::optional{Mesh::Data{
                    std::move(vertices),
                    std::move(indices)
                }};
            }

            // Сфера
            if (path.find(sphere_name) != std::string_view::npos){
                constexpr float size = 1.0f;
                constexpr float radius = size / 2.0f;
                constexpr int segments = 32; // Деление по долготе
                constexpr int rings = 16;    // Деление по широте

                auto* lp = load_params<MeshLoadParams>();
                const bool clockwise = !lp || !lp->winding_ccw;

                std::vector<rendering::Vertex> vertices;
                std::vector<uint32_t> indices;

                vertices.reserve(segments * rings * 6);
                indices.reserve(segments * rings * 6);

                // Генерация вершин
                for (int j = 0; j <= rings; ++j) {
                    constexpr float pi = 3.14159265359f;
                    float phi = pi * static_cast<float>(j) / rings; // Зенит (0 to π)
                    float sin_phi = std::sin(phi);
                    float cos_phi = std::cos(phi);

                    for (int i = 0; i <= segments; ++i) {
                        float theta = 2.0f * pi * static_cast<float>(i) / segments; // Азимут (0 to 2π)
                        float sin_theta = std::sin(theta);
                        float cos_theta = std::cos(theta);

                        // Позиция вершины
                        glm::vec3 pos = {
                            radius * sin_phi * cos_theta,
                            radius * sin_phi * sin_theta,
                            radius * cos_phi
                        };

                        // Нормаль (совпадает с позицией, нормализованная)
                        glm::vec3 normal = glm::normalize(pos);

                        // UV-координаты
                        float u = static_cast<float>(i) / segments; // 0 to 1
                        float v = static_cast<float>(j) / rings;   // 0 to 1

                        // Белый цвет вершины
                        glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

                        vertices.push_back({pos, normal, {u, v}, color});
                    }
                }

                // Генерация индексов
                for (int j = 0; j < rings; ++j) {
                    for (int i = 0; i < segments; ++i) {
                        uint32_t i0 = j * (segments + 1) + i;
                        uint32_t i1 = i0 + 1;
                        uint32_t i2 = (j + 1) * (segments + 1) + i;
                        uint32_t i3 = i2 + 1;

                        // Два треугольника на квадрат
                        if (clockwise) {
                            indices.insert(indices.end(), {i0, i1, i2});
                            indices.insert(indices.end(), {i1, i3, i2});
                        } else {
                            indices.insert(indices.end(), {i0, i2, i1});
                            indices.insert(indices.end(), {i1, i2, i3});
                        }
                    }
                }

                err_code_ = ErrorCode::eNoError;
                return std::optional{Mesh::Data{
                    std::move(vertices),
                    std::move(indices)
                }};
            }

            err_code_ = ErrorCode::eUnknownResource;
            return std::nullopt;
        }
    };
}