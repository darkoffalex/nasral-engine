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
        EnumMask<FrameAttachmentType> input_attachments = {};
        EnumMask<FrameAttachmentType> output_attachments = {};
    };

    struct UniformIndexComponent
    {
        uint32_t index = 0;
    };

    struct UniformStateComponent
    {
        bool is_dirty = false;
    };

    struct ShadowSettingsComponent
    {
        bool cast_shadow = false;
    };

    struct DirtyUnformComponent{};

    struct DirtyHandlesComponent{};

    struct DirtyTexturesComponent{};
}
