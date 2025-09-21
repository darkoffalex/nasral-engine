#pragma once
#include <nasral/rendering/rendering_types.h>

namespace nasral::rendering::components
{
    struct MaterialHandles
    {
        Handles::Material material_handles = {};
        std::array<Handles::Texture, static_cast<size_t>(TextureType::TOTAL)> texture_handles = {};
        std::array<TextureSamplerType, static_cast<size_t>(TextureType::TOTAL)> texture_samplers = {};
        std::array<bool, static_cast<size_t>(TextureType::TOTAL)> texture_dirty = {};
    };

    struct MaterialSettings
    {
        size_t index = 0;
        bool dirty = false;
        MaterialUniforms uniforms = {};
    };
}
