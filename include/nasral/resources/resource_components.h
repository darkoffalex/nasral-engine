#pragma once
#include <nasral/rendering/rendering_types.h>
#include <nasral/resources/request.h>

namespace nasral::resources::components
{
    struct MaterialRequest
    {
        bool needed = false;
        Request pipeline_request = {};
        std::array<Request, static_cast<size_t>(rendering::TextureType::TOTAL)> texture_requests = {};
    };

    struct MaterialDescriptors
    {
        std::string_view material_path = {};
        std::array<std::string_view, static_cast<size_t>(rendering::TextureType::TOTAL)> texture_paths = {};
    };

    struct MeshRequest
    {
        bool needed = false;
        Request request = {};
    };

    struct MeshDescriptor
    {
        std::string_view mesh_path = {};
    };
}
