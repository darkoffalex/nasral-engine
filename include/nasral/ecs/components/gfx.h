#pragma once

#include <nasral/gfx/types.h>

namespace nasral::ecs
{
    struct MaterialHandlesComponent
    {
        gfx::handles::Material material;
        EnumArray<gfx::TextureType, gfx::handles::Texture> textures;
    };

    struct MaterialSettingsComponent
    {
        gfx::MaterialBaseType base_type = gfx::MaterialBaseType::eDummy;
        gfx::uniforms::Material uniforms = {};
        EnumArray<gfx::TextureType, gfx::TextureSamplerType> samplers = {};
    };

    struct UniformIndexComponent
    {
        uint32_t index = 0;
    };

    struct UniformStateComponent
    {
        bool is_dirty = false;
    };

    struct DirtyUnformComponent{};

    struct DirtyTexturesComponent{};
}