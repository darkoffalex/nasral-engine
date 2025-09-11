#pragma once
#include <nasral/resources/request.h>

namespace nasral::resources::components
{
    struct TextureResource
    {
        Request texture;
    };

    struct TextureSetResources
    {
        Request color;
        Request normal;
        Request roughness;
        Request height;
        Request metallic;
    };

    struct MaterialResource
    {
        Request material;
    };

    struct MeshResource
    {
        Request mesh;
    };
}
