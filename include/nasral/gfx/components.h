#pragma once

#include <nasral/gfx/types.h>

namespace nasral::gfx
{
    struct RenderComponent{};

    struct MaterialHandlesComponent
    {
        handles::Material material = {};
        EnumArray<TextureType, handles::Texture> textures = {};
    };

    struct MaterialSettingsComponent
    {
        MaterialBaseType base_type = MaterialBaseType::eDummy;
        uniforms::Material uniforms = {};
        EnumArray<TextureType, TextureSamplerType> samplers = {};
    };

    struct MeshHandlesComponent
    {
        handles::Mesh mesh = {};
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

    struct DirtyHandlesComponent{};

    struct DirtyTexturesComponent{};
}
