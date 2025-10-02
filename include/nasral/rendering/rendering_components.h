#pragma once
#include <nasral/rendering/rendering_types.h>
#include <nasral/ecs/ecs_entity.h>

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
        uint32_t index = 0;
        bool dirty = false;
        MaterialUniforms uniforms = {};
    };

    struct MeshHandles
    {
        Handles::Mesh mesh_handles = {};
    };

    struct ObjectSettings
    {
        ecs::EntityId material_entity = {};
        uint32_t index = 0;
        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;
        bool dirty = false;
    };

    struct CameraSettings
    {
        glm::vec3 position = {};
        glm::vec3 rotation = {};
        glm::float32 fov = 60.0f;
        bool dirty = false;
    };

    struct LightSettings
    {
        uint32_t index = 0;
        bool dirty_state = false;
        bool dirty_settings = false;
        bool active = false;
        LightSettingsUniforms uniforms = {};
    };
}
