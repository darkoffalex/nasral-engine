#pragma once
#include <string>
#include <nasral/core/types.h>
#include <nasral/gfx/types.h>

namespace nasral::gfx::io
{
    struct Material
    {
        core::UniqueId id;
        MaterialType type;
        std::string material_path;
        core::EnumArray<TextureType, std::string> texture_paths;
        core::EnumArray<TextureType, TextureSamplerType> texture_samplers;
        uniforms::Material material_settings;
    };
}
