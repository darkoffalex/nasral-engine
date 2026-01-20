#pragma once

#include <glm/glm.hpp>
#include <nasral/core/types.h>
#include <nasral/scn/types.h>
#include <nasral/gfx/types.h>
#include <nasral/ecs/entity.h>
#include <nasral/core/component.h>

namespace nasral::scn::comp
{
    struct Node : core::Component<Node>
    {
        core::UniqueId uid;
        NodeType type = NodeType::eDummy;
        std::optional<ecs::EntityId> parent = std::nullopt;
    };

    struct NodeChildren : core::Component<NodeChildren>
    {
        ecs::EntityIdVector<4> children;
    };

    struct Spatial : core::Component<Spatial>
    {
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    };

    struct Camera : core::Component<Camera>
    {
        CameraType type = CameraType::ePerspective;
        glm::float32_t fov = 90.0f;
        glm::float32_t aspect = 1.0f;
        glm::float32_t near = 0.1f;
        glm::float32_t far = 1000.0f;
    };

    struct Mesh : core::Component<Mesh>
    {
        std::optional<ecs::EntityId> material_entity = std::nullopt;
    };

    struct Light : core::Component<Light>
    {
        gfx::LightType type = gfx::LightType::ePointLight;
        glm::float32_t intensity = 1.0f;
        glm::float32_t radius = 1.0f;
        glm::float32 quadratic = 0.1f;
        glm::vec4 color = glm::vec4(1.0f);
    };
}
