#pragma once

#include <nasral/gfx/types.h>

namespace nasral::gfx
{
    struct RenderComponent{};

    struct ShaderHandlesComponent
    {
        handles::ShaderProgram shader_program = {};
    };

    struct TextureHandlesComponent
    {
        EnumArray<TextureType, handles::Texture> textures = {};
    };

    struct MeshHandlesComponent
    {
        handles::Mesh mesh = {};
    };

    struct MaterialComponent
    {
        MaterialType type = MaterialType::eDummy;
        uniforms::Material uniforms = {};
        EnumArray<TextureType, TextureSamplerType> samplers = {};
    };

    struct ScreenPassComponent
    {
        ScreenPassType type = ScreenPassType::eCompositing;
        uniforms::ScreenPass uniforms = {};
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
