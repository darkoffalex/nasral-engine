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
                desc.pbr_settings.normal_mul = s.value("normal_mul", desc.pbr_settings.normal_mul);
            }
            else if (desc.type == MaterialType::ePBR)
            {
                desc.pbr_settings.color = s.value("color", desc.pbr_settings.color);
                desc.pbr_settings.roughness = s.value("roughness", desc.pbr_settings.roughness);
                desc.pbr_settings.metallic = s.value("metallic", desc.pbr_settings.metallic);
                desc.pbr_settings.ao = s.value("ao", desc.pbr_settings.ao);
                desc.pbr_settings.emission = s.value("emission", desc.pbr_settings.emission);
                desc.pbr_settings.normal_mul = s.value("normal_mul", desc.pbr_settings.normal_mul);
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
                {"specular", desc.phong_settings.specular},
                {"normal_mul", desc.phong_settings.normal_mul}
            };
        }
        else if (desc.type == MaterialType::ePBR)
        {
            j["settings"] = {
                {"color", desc.pbr_settings.color},
                {"roughness", desc.pbr_settings.roughness},
                {"metallic", desc.pbr_settings.metallic},
                {"ao", desc.pbr_settings.ao},
                {"emission", desc.pbr_settings.emission},
                {"normal_mul", desc.pbr_settings.normal_mul}
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

    inline void from_json(const nlohmann::json& j, ScreenPassDesc& desc)
    {
        desc.unique_id = j.value("uid", desc.unique_id);
        desc.name = j.value("name", desc.name);
        desc.type = j.value("type", ScreenPassType::eCompositing);

        if (j.contains("shader_program_path"))
        {
            desc.shader_program_path = j.at("shader_program_path").get<std::string>();
        }

        if (j.contains("input_attachments") && j.at("input_attachments").is_array())
        {
            desc.input_attachments = j.at("input_attachments").get<std::vector<FrameAttachmentType>>();
        }

        if (j.contains("output_attachments") && j.at("output_attachments").is_array())
        {
            desc.output_attachments = j.at("output_attachments").get<std::vector<FrameAttachmentType>>();
        }

        if (j.contains("previous") && !j.at("previous").is_null())
        {
            desc.previous = j.at("previous").get<UniqueId>();
        }

        if (j.contains("next") && !j.at("next").is_null())
        {
            desc.next = j.at("next").get<UniqueId>();
        }

        if (j.contains("settings") && j.at("settings").is_object())
        {
            const auto& s = j.at("settings");
            if (desc.type == ScreenPassType::eAO)
            {
                desc.ao_settings.type = s.value("type", desc.ao_settings.type);
                desc.ao_settings.samples = s.value("samples", desc.ao_settings.samples);
                desc.ao_settings.radius = s.value("radius", desc.ao_settings.radius);
                desc.ao_settings.bias = s.value("bias", desc.ao_settings.bias);
                desc.ao_settings.multiplier = s.value("multiplier", desc.ao_settings.multiplier);
                desc.ao_settings.power = s.value("power", desc.ao_settings.power);
            }
            else if (desc.type == ScreenPassType::eBlur)
            {
                desc.blur_settings.pass_index = s.value("pass_index", desc.blur_settings.pass_index);
                desc.blur_settings.samples = s.value("samples", desc.blur_settings.samples);
                desc.blur_settings.base_tex_radius = s.value("base_tex_radius", desc.blur_settings.base_tex_radius);
                desc.blur_settings.base_kernel_radius = s.value("base_kernel_radius", desc.blur_settings.base_kernel_radius);
                desc.blur_settings.direction = s.value("direction", desc.blur_settings.direction);
            }
            else if (desc.type == ScreenPassType::eCompositing)
            {
                desc.compositing_settings.bloom_blur_lod = s.value("bloom_blur_lod", desc.compositing_settings.bloom_blur_lod);
                desc.compositing_settings.bloom_intensity = s.value("bloom_intensity", desc.compositing_settings.bloom_intensity);
                desc.compositing_settings.gamma = s.value("gamma", desc.compositing_settings.gamma);
                desc.compositing_settings.exposure = s.value("exposure", desc.compositing_settings.exposure);
            }
        }
    }

    inline void to_json(nlohmann::json& j, const ScreenPassDesc& desc)
    {
        j = nlohmann::json{
            {"uid", desc.unique_id},
            {"name", desc.name},
            {"type", desc.type},
            {"shader_program_path", desc.shader_program_path},
            {"input_attachments", desc.input_attachments},
            {"output_attachments", desc.output_attachments}
        };

        if (desc.previous.has_value())
        {
            j["previous"] = desc.previous.value();
        }

        if (desc.next.has_value())
        {
            j["next"] = desc.next.value();
        }

        if (desc.type == ScreenPassType::eAO)
        {
            j["settings"] = {
                {"type", desc.ao_settings.type},
                {"samples", desc.ao_settings.samples},
                {"radius", desc.ao_settings.radius},
                {"bias", desc.ao_settings.bias},
                {"multiplier", desc.ao_settings.multiplier},
                {"power", desc.ao_settings.power}
            };
        }
        else if (desc.type == ScreenPassType::eBlur)
        {
            j["settings"] = {
                {"pass_index", desc.blur_settings.pass_index},
                {"samples", desc.blur_settings.samples},
                {"base_tex_radius", desc.blur_settings.base_tex_radius},
                {"base_kernel_radius", desc.blur_settings.base_kernel_radius},
                {"direction", desc.blur_settings.direction}
            };
        }
        else if (desc.type == ScreenPassType::eCompositing)
        {
            j["settings"] = {
                {"bloom_blur_lod", desc.compositing_settings.bloom_blur_lod},
                {"bloom_intensity", desc.compositing_settings.bloom_intensity},
                {"gamma", desc.compositing_settings.gamma},
                {"exposure", desc.compositing_settings.exposure}
            };
        }
    }
}