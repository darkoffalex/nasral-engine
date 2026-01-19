#pragma once
#include <nasral/res/resources/mesh.h>

namespace nasral::res
{
    class MeshBuiltinLoader final : public Loader<Mesh::Data>
    {
    public:
        explicit MeshBuiltinLoader(Engine* const engine) : Loader(engine)
        {}

        std::optional<Mesh::Data> load(const std::string_view& path) override
        {
            // Квадрат
            if (path.find(kBuiltinMeshQuad) != std::string::npos){
                constexpr float size = 1.0f;
                constexpr float half_size = size / 2.0f;

                std::vector<gfx::Vertex> vertices = {
                    {{-half_size, -half_size, 0.0f},{0.0f, 0.0f, 1.0f},{0.0f, 0.0f},{1.0f, 0.0f,0.0f,1.0f}},
                    {{-half_size, half_size, 0.0f},{0.0f, 0.0f, 1.0f},{0.0f, 1.0f},{0.0f, 1.0f,0.0f,1.0f}},
                    {{half_size, half_size, 0.0f},{0.0f, 0.0f, 1.0f},{1.0f, 1.0f},{0.0f, 0.0f,1.0f,1.0f}},
                    {{half_size, -half_size, 0.0f},{0.0f, 0.0f, 1.0f},{1.0f, 0.0f},{1.0f, 1.0f,0.0f,1.0f}}
                };
                std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

                error_ = Error::eNone;
                return std::optional{Mesh::Data{
                    std::move(vertices),
                    std::move(indices)
                }};
            }

            // Куб
            if (path.find(kBuiltinMeshCube) != std::string::npos){
                constexpr float size = 1.0f;
                constexpr float half_size = size / 2.0f;

                std::vector<gfx::Vertex> vertices;
                std::vector<uint32_t> indices;

                for (size_t f = 0; f < 6; ++f)
                {
                    glm::vec3 axis;
                    glm::float32 angle;

                    // Первые 4 грани вращаем по оси Y (0, 90, 180, 270)
                    if (f < 4) {
                        axis = {0.0f, 1.0f, 0.0f};
                        angle = glm::radians(f * 90.0f);
                    }
                    // Последние 2 грани вращаем по оси X (90, 270)
                    else {
                        axis = {1.0f, 0.0f, 0.0f};
                        angle = glm::radians(f == 4 ? 90.0f : 270.0f);
                    }

                    // Создаем матрицу поворота
                    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, axis);

                    // Запоминаем текущий индекс начала вершин для этой грани
                    const uint32_t base_index = static_cast<uint32_t>(vertices.size());

                    // Трансформируем и добавляем вершины
                    for (int i = 0; i < 4; ++i)
                    {
                        // Положение
                        constexpr glm::vec3 positions[4] = {
                            {-half_size, -half_size, half_size},
                            {-half_size, half_size, half_size},
                            {half_size, half_size, half_size},
                            {half_size, -half_size, half_size}
                        };

                        // Нормали
                        constexpr glm::vec3 normals[4] = {
                            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
                            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}
                        };

                        // UV координаты
                        constexpr glm::vec2 uvs[4] = {
                            {0.0f, 0.0f}, {0.0f, 1.0f},
                            {1.0f, 1.0f}, {1.0f, 0.0f}
                        };

                        // Цвета
                        constexpr glm::vec4 colors[4] = {
                            {1.0f, 0.0f,0.0f,1.0f}, {0.0f, 1.0f,0.0f,1.0f},
                            {0.0f, 0.0f,1.0f,1.0f}, {1.0f, 1.0f,0.0f,1.0f}
                        };

                        // Вращаем позицию (w=1.0 для точки)
                        const auto pos = glm::vec3(rotation * glm::vec4(positions[i], 1.0f));
                        // Вращаем нормаль (w=0.0 для вектора направления)
                        const auto norm = glm::vec3(rotation * glm::vec4(normals[i], 0.0f));
                        // Добавить вершину
                        vertices.push_back({pos, norm, uvs[i], colors[i]});
                    }

                    // Добавляем индексы для двух треугольников грани: 0-1-2 и 2-3-0
                    indices.push_back(base_index + 0);
                    indices.push_back(base_index + 1);
                    indices.push_back(base_index + 2);
                    indices.push_back(base_index + 2);
                    indices.push_back(base_index + 3);
                    indices.push_back(base_index + 0);
                }

                error_ = Error::eNone;
                return std::optional{Mesh::Data{
                    std::move(vertices),
                    std::move(indices)
                }};
            }

            // Сфера
            if (path.find(kBuiltinMeshSphere) != std::string::npos){
                constexpr float size = 1.0f;
                constexpr float radius = size / 2.0f;
                constexpr int segments = 32; // Деление по долготе
                constexpr int rings = 16;    // Деление по широте

                auto* lp = load_params<MeshLoadParams>();
                const bool clockwise = !lp || !lp->winding_order_ccw;

                std::vector<gfx::Vertex> vertices;
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

                error_ = Error::eNone;
                return std::optional{Mesh::Data{
                    std::move(vertices),
                    std::move(indices)
                }};
            }

            // Указан неизвестный идентификатор ресурса
            error_ = Error::eUnknownResource;
            return std::nullopt;
        }
    };
}