#pragma once

#include <nlohmann/json.hpp>
#include <nasral/res/types.h>

#include "./json_common.hpp"
#include "./json_glm.hpp"

namespace nasral::res
{
    inline void from_json(const nlohmann::json& j, ResourceDesc& desc)
    {
        const auto type = j.value("type", Type::eUndefined);
        const auto path = j.value("path", "");
        std::optional<LoadParams> params = std::nullopt;

        if (j.contains("load_params") && j.at("load_params").is_object() && !j.at("load_params").empty())
        {
            const auto& lp = j.at("load_params");
            switch (type)
            {
                case Type::eTexture:
                {
                    TextureLoadParams tlp;
                    tlp.srgb = lp.value("srgb", tlp.srgb);
                    tlp.generate_mipmaps = lp.value("generate_mipmaps", tlp.generate_mipmaps);
                    params = tlp;
                    break;
                }
                case Type::eMesh:
                {
                    MeshLoadParams mlp;
                    mlp.generate_tangents = lp.value("generate_tangents", mlp.generate_tangents);
                    mlp.generate_normals = lp.value("generate_normals", mlp.generate_normals);
                    mlp.winding_order_ccw = lp.value("winding_order_ccw", mlp.winding_order_ccw);
                    params = mlp;
                    break;
                }
                case Type::eShaderProgram:
                {
                    ShaderLoadParams slp;
                    slp.type = lp.value("type", ShaderLoadParams::eRasterization);
                    params = slp;
                    break;
                }
                default:
                    break;
            }
        }

        desc = {type, path, params};
    }

    inline void to_json(nlohmann::json& j, const ResourceDesc& desc)
    {
        const auto& [type, path, params] = desc;
        j = nlohmann::json{
            {"type", type},
            {"path", path}
        };

        if (params.has_value())
        {
            std::visit([&j](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, TextureLoadParams>)
                {
                    j["load_params"] = {
                        {"srgb", p.srgb},
                        {"generate_mipmaps", p.generate_mipmaps}
                    };
                }
                else if constexpr (std::is_same_v<T, MeshLoadParams>)
                {
                    j["load_params"] = {
                        {"generate_tangents", p.generate_tangents},
                        {"generate_normals", p.generate_normals},
                        {"winding_order_ccw", p.winding_order_ccw}
                    };
                }
                else if constexpr (std::is_same_v<T, ShaderLoadParams>)
                {
                    j["load_params"] = {
                        {"type", p.type}
                    };
                }
            }, params.value());
        }
        else
        {
            j["load_params"] = nlohmann::json::object();
        }
    }
}

template <>
struct nlohmann::adl_serializer<nasral::res::ResourceDesc>
{
    static void to_json(json& j, const nasral::res::ResourceDesc& desc)
    {
        nasral::res::to_json(j, desc);
    }

    static void from_json(const json& j, nasral::res::ResourceDesc& desc)
    {
        nasral::res::from_json(j, desc);
    }

    static nasral::res::ResourceDesc from_json(const json& j)
    {
        nasral::res::ResourceDesc desc;
        nasral::res::from_json(j, desc);
        return desc;
    }
};
