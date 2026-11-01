#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace glm 
{
    // ==========================================
    // glm::vec2
    // ==========================================
    inline void to_json(nlohmann::json& j, const vec2& v)
    {
        j = nlohmann::json::array({ v.x, v.y });
    }

    inline void from_json(const nlohmann::json& j, vec2& v)
    {
        if (j.is_array())
        {
            if (j.size() >= 2)
            {
                v.x = j[0].get<float>();
                v.y = j[1].get<float>();
            }
        }
        else if (j.is_object())
        {
            v.x = j.value("x", v.x);
            v.y = j.value("y", v.y);
        }
    }

    // ==========================================
    // glm::vec3
    // ==========================================
    inline void to_json(nlohmann::json& j, const vec3& v)
    {
        j = nlohmann::json::array({ v.x, v.y, v.z });
    }

    inline void from_json(const nlohmann::json& j, vec3& v)
    {
        if (j.is_array())
        {
            if (j.size() >= 3)
            {
                v.x = j[0].get<float>();
                v.y = j[1].get<float>();
                v.z = j[2].get<float>();
            }
        }
        else if (j.is_object())
        {
            v.x = j.value("x", v.x);
            v.y = j.value("y", v.y);
            v.z = j.value("z", v.z);
        }
    }

    // ==========================================
    // glm::vec4
    // ==========================================
    inline void to_json(nlohmann::json& j, const vec4& v)
    {
        j = nlohmann::json::array({ v.x, v.y, v.z, v.w });
    }

    inline void from_json(const nlohmann::json& j, vec4& v)
    {
        if (j.is_array())
        {
            if (j.size() >= 4)
            {
                v.x = j[0].get<float>();
                v.y = j[1].get<float>();
                v.z = j[2].get<float>();
                v.w = j[3].get<float>();
            }
        }
        else if (j.is_object())
        {
            v.x = j.value("x", v.x);
            v.y = j.value("y", v.y);
            v.z = j.value("z", v.z);
            v.w = j.value("w", v.w);
        }
    }

    // ==========================================
    // glm::mat3 (column-major)
    // ==========================================
    inline void to_json(nlohmann::json& j, const mat3& m)
    {
        j = nlohmann::json::array({
            m[0][0], m[0][1], m[0][2],
            m[1][0], m[1][1], m[1][2],
            m[2][0], m[2][1], m[2][2]
        });
    }

    inline void from_json(const nlohmann::json& j, mat3& m)
    {
        if (j.is_array())
        {
            // Плоский массив из 9 элементов
            if (j.size() == 9)
            {
                for (int c = 0; c < 3; ++c)
                {
                    for (int r = 0; r < 3; ++r)
                    {
                        m[c][r] = j[c * 3 + r].get<float>();
                    }
                }
            }
            // Массив из 3 векторов-столбцов
            else if (j.size() == 3)
            {
                for (int c = 0; c < 3; ++c)
                {
                    m[c] = j[c].get<vec3>();
                }
            }
        }
    }

    // ==========================================
    // glm::mat4 (column-major)
    // ==========================================
    inline void to_json(nlohmann::json& j, const mat4& m)
    {
        j = nlohmann::json::array({
            m[0][0], m[0][1], m[0][2], m[0][3],
            m[1][0], m[1][1], m[1][2], m[1][3],
            m[2][0], m[2][1], m[2][2], m[2][3],
            m[3][0], m[3][1], m[3][2], m[3][3]
        });
    }

    inline void from_json(const nlohmann::json& j, mat4& m)
    {
        if (j.is_array())
        {
            // Плоский массив из 16 элементов
            if (j.size() == 16)
            {
                for (int c = 0; c < 4; ++c)
                {
                    for (int r = 0; r < 4; ++r)
                    {
                        m[c][r] = j[c * 4 + r].get<float>();
                    }
                }
            }
            // Массив из 4 векторов-столбцов
            else if (j.size() == 4)
            {
                for (int c = 0; c < 4; ++c)
                {
                    m[c] = j[c].get<vec4>();
                }
            }
        }
    }
}
