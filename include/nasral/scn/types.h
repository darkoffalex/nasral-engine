#pragma once

#include <nasral/common/types.h>
#include <nasral/gfx/types.h>
#include <nasral/ecs/entity.h>
#include <glm/glm.hpp>

namespace nasral::scn
{
    enum class NodeType : uint32_t
    {
        eDummy = 0,
        eSpatial,
        eCamera,
        eMesh,
        eSprite,
        eLight,
        TOTAL
    };

    namespace data
    {
        struct DummyNodeView
        {
            const UniqueId& uid;
            const std::string& name;
            const NodeType& type;
        };

        struct SpatialNodeView : DummyNodeView
        {
            const glm::vec3& position;
            const glm::vec3& rotation;
            const glm::vec3& scale;
        };

        struct CameraNodeView : SpatialNodeView
        {
            const gfx::ViewType viewType;
            const glm::float32_t& fov;
            const glm::float32_t& aspect;
            const glm::float32_t& near;
            const glm::float32_t& far;
        };

        struct LightNodeView : SpatialNodeView
        {
            const gfx::LightType& type;
            const glm::float32_t& intensity;
            const glm::float32_t& radius;
            const glm::float32& quadratic;
            const glm::vec4& color;
        };

        struct MeshNodeView : SpatialNodeView
        {
            const ecs::EntityIds<gfx::kMaxMaterialsPerMesh> materials;
        };

        using NodeView = std::variant<
            DummyNodeView,
            SpatialNodeView,
            CameraNodeView,
            MeshNodeView
        >;
    }

    struct NodeDesc
    {
        UniqueId unique_id = {};
        NodeType type = NodeType::eDummy;
        std::string name = {};

        struct
        {
            glm::vec3 position = {1.0f, 1.0f, 1.0f};
            glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
            glm::vec3 scale = {1.0f, 1.0f, 1.0f};
        } spatial = {};

        struct
        {
            gfx::ViewType type = gfx::ViewType::ePerspective;
            glm::float32_t fov = 90.0f;
            glm::float32_t aspect = 1.0f;
            glm::float32_t near = 0.1f;
            glm::float32_t far = 1000.0f;
        } camera = {};

        struct
        {
            gfx::LightType type = gfx::LightType::ePointLight;
            glm::float32_t intensity = 1.0f;
            glm::float32_t radius = 1.0f;
            glm::float32 quadratic = 0.1f;
            glm::vec4 color = glm::vec4(1.0f);
        } light = {};

        struct
        {
            std::vector<UniqueId> materials;
        } mesh = {};
    };

    struct Config
    {
    };
}
