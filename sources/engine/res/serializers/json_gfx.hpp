#pragma once

#include <nlohmann/json.hpp>
#include <nasral/gfx/types.h>

#include "./json_common.hpp"
#include "./json_glm.hpp"

namespace nasral::gfx
{
        inline void from_json(const nlohmann::json& j, MaterialDesc& desc)
    {
        desc.unique_id = j.value("uid", desc.unique_id);
        desc.name = j.value("name", desc.name);

        if (j.contains("shader_program_path"))
        {
            desc.shader_program_path = j.at("shader_program_path").get<std::string>();
        }

        desc.type = j.value("type", MaterialType::eDummy);

        if (j.contains("settings") && j.at("settings").is_object())
        {
            const auto& s = j.at("settings");
            if (desc.type == MaterialType::ePhong)
            {
                desc.phong_settings.color = s.value("color", desc.phong_settings.color);
                desc.phong_settings.ambient = s.value("ambient", desc.phong_settings.ambient);
                desc.phong_settings.shininess = s.value("shininess", desc.phong_settings.shininess);
                desc.phong_settings.specular = s.value("specular", desc.phong_settings.specular);
            }
            else if (desc.type == MaterialType::ePBR)
            {
                desc.pbr_settings.color = s.value("color", desc.pbr_settings.color);
                desc.pbr_settings.roughness = s.value("roughness", desc.pbr_settings.roughness);
                desc.pbr_settings.metallic = s.value("metallic", desc.pbr_settings.metallic);
                desc.pbr_settings.ao = s.value("ao", desc.pbr_settings.ao);
                desc.pbr_settings.emission = s.value("emission", desc.pbr_settings.emission);
            }
        }

        if (j.contains("textures") && j.at("textures").is_array())
        {
            for (const auto& tex_entry : j.at("textures"))
            {
                if (!tex_entry.is_object()) continue;
                if (const auto tex_type = tex_entry.value("type", TextureType::TOTAL); tex_type != TextureType::TOTAL)
                {
                    desc.texture_paths[tex_type] = tex_entry.value("path", "");
                    desc.texture_samplers[tex_type] = tex_entry.value("filter", TextureSamplerType::eNearest);
                }
            }
        }
    }

    inline void to_json(nlohmann::json& j, const MaterialDesc& desc)
    {
        j = nlohmann::json{
            {"uid", desc.unique_id},
            {"name", desc.name},
            {"type", desc.type},
            {"shader_program_path", desc.shader_program_path}
        };

        if (desc.type == MaterialType::ePhong)
        {
            j["settings"] = {
                {"color", desc.phong_settings.color},
                {"ambient", desc.phong_settings.ambient},
                {"shininess", desc.phong_settings.shininess},
                {"specular", desc.phong_settings.specular}
            };
        }
        else if (desc.type == MaterialType::ePBR)
        {
            j["settings"] = {
                {"color", desc.pbr_settings.color},
                {"roughness", desc.pbr_settings.roughness},
                {"metallic", desc.pbr_settings.metallic},
                {"ao", desc.pbr_settings.ao},
                {"emission", desc.pbr_settings.emission}
            };
        }

        nlohmann::json textures_arr = nlohmann::json::array();
        for (const auto tex_type : magic_enum::enum_values<TextureType>())
        {
            if (tex_type == TextureType::TOTAL) continue;
            if (!desc.texture_paths[tex_type].empty())
            {
                textures_arr.push_back({
                    {"type", tex_type},
                    {"path", desc.texture_paths[tex_type]},
                    {"filter", desc.texture_samplers[tex_type]}
                });
            }
        }
        if (!textures_arr.empty())
        {
            j["textures"] = textures_arr;
        }
    }
}