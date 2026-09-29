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
        std::array<bool, gfx::kMaxMaterialsPerMesh> materials_requested = {false};
    };

    struct LightComponent
    {
        gfx::LightType type = gfx::LightType::ePointLight;
        glm::float32_t intensity = 1.0f;
        glm::float32_t radius = 1.0f;
        glm::float32 quadratic = 0.1f;
        glm::vec4 color = glm::vec4(1.0f);
    };

    struct ScreenFxComponent
    {
        ecs::EntityId screen_fx = ecs::EntityId::invalid();
        ecs::EntityId screen_fx_prev = ecs::EntityId::invalid();
        bool screen_fx_requested = false;
        bool screen_fx_prev_released = false;
        gfx::ScreenFxAoType ao_type = gfx::ScreenFxAoType::eSSAO;
        glm::float32 ao_radius = 0.3f;
        glm::float32 ao_bias = 0.02f;
        glm::float32 ao_multiplier = 1.0f;
        glm::float32 ao_power_pre = 1.0f;
        glm::float32 ao_power_post = 1.0f;
        glm::uint32 blur_samples = 16;
        glm::float32 blur_base_tex_radius = 2.0f;
        glm::float32 blur_base_kernel_radius = 6.0f;
        glm::float32 bloom_blur_lod = 2.5f;
        glm::float32 bloom_intensity = 1.0f;
        glm::float32 gamma = 2.2f;
        glm::float32 exposure = 1.0f;
    };

    struct DirtyStateComponent {};
}
