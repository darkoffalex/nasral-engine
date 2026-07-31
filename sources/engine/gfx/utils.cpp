#include "pch.h"
#include <nasral/gfx/utils.h>
#include <nasral/res/manager.h>
#include <nasral/res/objects/texture.h>

namespace nasral::gfx
{
    vk::Format get_image_vk_format(const uint32_t channel_count, const uint32_t channel_depth, const bool srgb)
    {
        // 1 байт (8 бит) на канал
        if (channel_depth == 1) {
            switch (channel_count) {
                case 1: return srgb ? vk::Format::eR8Srgb : vk::Format::eR8Unorm;
                case 2: return srgb ? vk::Format::eR8G8Srgb : vk::Format::eR8G8Unorm;
                case 3: return srgb ? vk::Format::eR8G8B8Srgb : vk::Format::eR8G8B8Unorm;
                case 4: return srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
                default: return vk::Format::eUndefined;
            }
        }
        // 2 байта (16 бит) на канал
        if (channel_depth == 2) { // 16-bit per channel
            switch (channel_count) {
                case 1: return vk::Format::eR16Unorm;
                case 2: return vk::Format::eR16G16Unorm;
                case 3: return vk::Format::eR16G16B16Unorm;
                case 4: return vk::Format::eR16G16B16A16Unorm;
                default: return vk::Format::eUndefined;
            }
        }
        // 4 байта (32 бит) на канал (HDR текстуры)
        if (channel_depth == 4) {
            switch (channel_count) {
                case 1: return vk::Format::eR32Sfloat;
                case 2: return vk::Format::eR32G32Sfloat;
                case 3: return vk::Format::eR32G32B32Sfloat;
                case 4: return vk::Format::eR32G32B32A32Sfloat;
                default: return vk::Format::eUndefined;
            }
        }
        return vk::Format::eUndefined;
    }

    GeometryData gen_quad_geometry(const float size)
    {
        const float half_size = size / 2.0f;

        std::vector<Vertex> vertices = {
            {{-half_size, -half_size, 0.0f},{0.0f, 0.0f, 1.0f},{0.0f, 0.0f},{1.0f, 0.0f,0.0f,1.0f}},
            {{-half_size, half_size, 0.0f},{0.0f, 0.0f, 1.0f},{0.0f, 1.0f},{0.0f, 1.0f,0.0f,1.0f}},
            {{half_size, half_size, 0.0f},{0.0f, 0.0f, 1.0f},{1.0f, 1.0f},{0.0f, 0.0f,1.0f,1.0f}},
            {{half_size, -half_size, 0.0f},{0.0f, 0.0f, 1.0f},{1.0f, 0.0f},{1.0f, 1.0f,0.0f,1.0f}}
        };

        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0
        };

        return std::make_tuple(
            std::move(vertices),
            std::move(indices)
        );
    }

    GeometryData gen_cube_geometry(const float size)
    {
        const float half_size = size / 2.0f;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(24);
        indices.reserve(36);

        for (size_t f = 0; f < 6; ++f)
        {
            glm::vec3 axis;
            glm::float32 angle;

            // Первые 4 грани вращаем по оси Y (0, 90, 180, 270)
            if (f < 4) {
                axis = {0.0f, 1.0f, 0.0f};
                angle = glm::radians(static_cast<float>(f) * 90.0f);
            }
            // Последние 2 грани вращаем по оси X (90, 270)
            else {
                axis = {1.0f, 0.0f, 0.0f};
                angle = glm::radians(f == 4 ? 90.0f : 270.0f);
            }

            // Создаем матрицу поворота
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, axis);

            // Запоминаем текущий индекс начала вершин для этой грани
            const auto base_index = static_cast<uint32_t>(vertices.size());

            // Трансформируем и добавляем вершины
            for (int i = 0; i < 4; ++i)
            {
                // Положение
                const glm::vec3 positions[4] = {
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

        return std::make_tuple(
            std::move(vertices),
            std::move(indices)
        );
    }

    GeometryData gen_sphere_geometry(
        const float radius,
        const uint32_t segments,
        const uint32_t rings,
        const bool clockwise,
        const glm::vec4& color)
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(segments * rings * 6);
        indices.reserve(segments * rings * 6);

        // Генерация вершин
        for (uint32_t j = 0; j <= rings; ++j) {
            constexpr float pi = 3.14159265359f;
            const float phi = pi * static_cast<float>(j) / static_cast<float>(rings); // Зенит (0 to π)
            const float sin_phi = std::sin(phi);
            const float cos_phi = std::cos(phi);

            for (uint32_t i = 0; i <= segments; ++i) {
                const float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments); // Азимут (0 to 2π)
                const float sin_theta = std::sin(theta);
                const float cos_theta = std::cos(theta);

                // Позиция вершины
                glm::vec3 pos = {
                    radius * sin_phi * cos_theta,
                    radius * sin_phi * sin_theta,
                    radius * cos_phi
                };

                // Нормаль (совпадает с позицией, нормализованная)
                const glm::vec3 normal = glm::normalize(pos);

                // UV-координаты
                float u = static_cast<float>(i) / static_cast<float>(segments); // 0 to 1
                float v = static_cast<float>(j) / static_cast<float>(rings);   // 0 to 1

                // Добавить вершину
                vertices.push_back({pos, normal, {u, v}, color});
            }
        }

        // Генерация индексов
        for (uint32_t j = 0; j < rings; ++j) {
            for (uint32_t i = 0; i < segments; ++i) {
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

        return std::make_tuple(
            std::move(vertices),
            std::move(indices)
        );
    }

    handles::Texture fallback_tex_h(const res::Manager* res_m, const TextureType type)
    {
        const auto fallback_tex_id = res_m->find_texture_fallback(type);
        const auto fallback_tex_res = res_m->get<res::Texture>(fallback_tex_id.value_or(res::kInvalidResourceId));

        if (!fallback_tex_res || fallback_tex_res->status() != res::Status::eLoaded) {
            return {};
        }

        return fallback_tex_res->render_handles();
    }
}
