#pragma once
#include <nasral/rendering/rendering_types.h>

namespace nasral::rendering::components
{
    struct TextureHandle
    {
        Handles::Texture texture;
    };

    struct TextureSetHandles
    {
        Handles::Texture color;
        Handles::Texture normal;
        Handles::Texture roughness;
        Handles::Texture height;
        Handles::Texture metallic;
    };

    struct MaterialHandle
    {
        Handles::Material material;
    };

    struct MeshHandle
    {
        Handles::Mesh mesh;
    };
}
