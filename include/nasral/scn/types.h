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
            const gfx::ViewType view_type;
            const glm::float32_t& fov;
            const glm::float32_t& aspect;
            const glm::float32_t& near;
            const glm::float32_t& far;
        };

        struct LightNodeView : SpatialNodeView
        {
            const gfx::LightType& light_type;
            const glm::float32_t& intensity;
            const glm::float32_t& radius;
            const glm::float32& quadratic;
            const glm::vec4& color;
        };

        struct MeshNodeView : SpatialNodeView
        {
            const ecs::EntityIds<gfx::kMaxMaterialsPerMesh>& materials;
        };

        using NodeView = std::variant<
            DummyNodeView,
            SpatialNodeView,
            CameraNodeView,
            MeshNodeView,
            LightNodeView
        >;

        struct ScreenFxStateView
        {
            const UniqueId& uid;
            const gfx::ScreenFxAoType& ao_type;
            const glm::float32& ao_radius;
            const glm::float32& ao_bias;
            const glm::float32& ao_multiplier;
            const glm::float32& ao_power_pre;
            const glm::float32& ao_power_post;
            const glm::uint32& blur_samples;
            const glm::float32& blur_base_tex_radius;
            const glm::float32& blur_base_kernel_radius;
            const glm::float32& bloom_blur_lod;
            const glm::float32& bloom_intensity;
            const glm::float32& gamma;
            const glm::float32& exposure;
        };
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
            bool dynamic = false;
            bool is_active = true;
            gfx::LightType type = gfx::LightType::ePointLight;
            glm::float32_t intensity = 1.0f;
            glm::float32_t radius = 1.0f;
            glm::float32 quadratic = 0.1f;
            glm::vec4 color = glm::vec4(1.0f);
        } light = {};

        struct
        {
            bool dynamic = false;
            std::vector<UniqueId> materials;
            std::string mesh_path = {};
        } mesh = {};
    };

    struct ScreenFxStateDesc
    {
        UniqueId unique_id = {};
        UniqueId screen_fx_uid = {};

        struct
        {
            gfx::ScreenFxAoType type = gfx::ScreenFxAoType::eSSAO;
            glm::float32 radius = 0.3f;
            glm::float32 bias = 0.02f;
            glm::float32 multiplier = 1.0f;
            glm::float32 power_pre = 1.0f;
            glm::float32 power_post = 1.0f;
        } ao;

        struct
        {
            glm::uint32 samples = 16;
            glm::float32 base_tex_radius = 2.0f;
            glm::float32 base_kernel_radius = 6.0f;
        } blur;

        struct
        {
            glm::float32 blur_lod = 2.5f;
            glm::float32 intensity = 1.0f;
        } bloom;

        struct
        {
            glm::float32 gamma = 2.2f;
            glm::float32 exposure = 1.0f;
        } final;
    };

    struct Config
    {
        uint32_t initial_node_count = 100;
    };
}
