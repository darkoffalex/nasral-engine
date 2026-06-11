#pragma once

#include <glm/glm.hpp>
#include <nasral/gfx/types.h>
#include <nasral/scn/types.h>
#include <nasral/ecs/entity.h>

namespace nasral::scn
{
    struct NodeComponent
    {
        NodeType type = NodeType::eDummy;
    };

    struct SpatialComponent
    {
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};
    };

    struct ViewComponent
    {
        gfx::ViewType type = gfx::ViewType::ePerspective;
        glm::float32_t fov = 90.0f;
        glm::float32_t aspect = 1.0f;
        glm::float32_t near = 0.1f;
        glm::float32_t far = 1000.0f;
    };

    struct MeshComponent
    {
        ecs::EntityIds<gfx::kMaxMaterialsPerMesh> materials = {};
    };

    struct LightComponent
    {
        gfx::LightType type = gfx::LightType::ePointLight;
        glm::float32_t intensity = 1.0f;
        glm::float32_t radius = 1.0f;
        glm::float32 quadratic = 0.1f;
        glm::vec4 color = glm::vec4(1.0f);
    };
}
