#pragma once

#include <nlohmann/json.hpp>
#include <nasral/scn/types.h>

#include "./json_common.hpp"
#include "./json_glm.hpp"

namespace nasral::scn
{
    inline void from_json(const nlohmann::json& j, NodeDesc& desc)
    {
        desc.unique_id = j.value("uid", desc.unique_id);
        desc.type = j.value("type", desc.type);
        desc.name = j.value("name", desc.name);

        if (j.contains("spatial") && j.at("spatial").is_object())
        {
            const auto& spatial = j.at("spatial");
            desc.spatial.position = spatial.value("position", desc.spatial.position);
            desc.spatial.rotation = spatial.value("rotation", desc.spatial.rotation);
            desc.spatial.scale = spatial.value("scale", desc.spatial.scale);
        }

        if (j.contains("camera") && j.at("camera").is_object())
        {
            const auto& camera = j.at("camera");
            desc.camera.type = camera.value("type", desc.camera.type);
            desc.camera.fov = camera.value("fov", desc.camera.fov);
            desc.camera.aspect = camera.value("aspect", desc.camera.aspect);
            desc.camera.near = camera.value("near", desc.camera.near);
            desc.camera.far = camera.value("far", desc.camera.far);
        }

        if (j.contains("light") && j.at("light").is_object())
        {
            const auto& light = j.at("light");
            desc.light.dynamic = light.value("dynamic", desc.light.dynamic);
            desc.light.is_active = light.value("is_active", desc.light.is_active);
            desc.light.type = light.value("type", desc.light.type);
            desc.light.intensity = light.value("intensity", desc.light.intensity);
            desc.light.radius = light.value("radius", desc.light.radius);
            desc.light.quadratic = light.value("quadratic", desc.light.quadratic);
            desc.light.color = light.value("color", desc.light.color);
        }

        if (j.contains("mesh") && j.at("mesh").is_object())
        {
            const auto& mesh = j.at("mesh");
            desc.mesh.dynamic = mesh.value("dynamic", desc.mesh.dynamic);
            desc.mesh.materials = mesh.value("materials", desc.mesh.materials);
            desc.mesh.mesh_path = mesh.value("mesh_path", desc.mesh.mesh_path);
            desc.mesh.cast_shadow = mesh.value("cast_shadow", desc.mesh.cast_shadow);
        }
    }

    inline void to_json(nlohmann::json& j, const NodeDesc& desc)
    {
        j = nlohmann::json{
            {"uid", desc.unique_id},
            {"type", desc.type},
            {"name", desc.name},
            {"spatial", {
                {"position", desc.spatial.position},
                {"rotation", desc.spatial.rotation},
                {"scale", desc.spatial.scale}
            }},
            {"camera", {
                {"type", desc.camera.type},
                {"fov", desc.camera.fov},
                {"aspect", desc.camera.aspect},
                {"near", desc.camera.near},
                {"far", desc.camera.far}
            }},
            {"light", {
                {"dynamic", desc.light.dynamic},
                {"is_active", desc.light.is_active},
                {"type", desc.light.type},
                {"intensity", desc.light.intensity},
                {"radius", desc.light.radius},
                {"quadratic", desc.light.quadratic},
                {"color", desc.light.color}
            }},
            {"mesh", {
                {"dynamic", desc.mesh.dynamic},
                {"materials", desc.mesh.materials},
                {"mesh_path", desc.mesh.mesh_path},
                {"cast_shadow", desc.mesh.cast_shadow}
            }}
        };
    }
}
