#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.hpp>

namespace nasral::gfx
{
    constexpr uint32_t kVkAppVersion = VK_MAKE_VERSION(1, 0, 0);
    constexpr uint32_t kVkEngineVersion = VK_MAKE_VERSION(1, 0, 0);

    constexpr uint32_t kMaxCameras = 1;
    constexpr uint32_t kMaxObjects = 1024;
    constexpr uint32_t kMaxMaterials = 100;
    constexpr uint32_t kMaxLights = 64;

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 color;
    };

    enum class UniformLayoutType : uint32_t
    {
        eDummy = 0,
        eBasicRasterization,
        ePostProcessing,
        TOTAL
    };

    enum class TextureSamplerType : uint32_t
    {
        eNearest = 0,
        eNearestClamp,
        eLinear,
        eLinearClamp,
        eAnisotropic,
        eAnisotropicClamp,
        TOTAL
    };

    enum class TextureType : uint32_t
    {
        eAlbedoColor = 0,
        eNormal,
        eRoughnessOrSpecular,
        eHeight,
        eMetallicOrReflection,
        eAmbientOcclusion,
        eEmission,
        TOTAL
    };

    enum class MaterialType : uint32_t
    {
        eDummy = 0,
        eVertexColored,
        eTextured,
        ePhong,
        ePbr,
        TOTAL
    };

    enum class LightType : uint32_t
    {
        ePointLight = 0,
        eDirectionalLight,
        eSpotLight,
        TOTAL
    };

    struct VulkanSurfaceProvider
    {
        typedef std::shared_ptr<VulkanSurfaceProvider> Ptr;
        virtual ~VulkanSurfaceProvider() = default;
        virtual VkSurfaceKHR create_surface(const vk::Instance& instance) = 0;
        virtual const std::vector<const char*>& extensions() = 0;
    };

    typedef vk::UniqueHandle<vk::DebugReportCallbackEXT, vk::detail::DispatchLoaderDynamic> VkDebugReportCallback;

    namespace handles
    {
        struct Material
        {
            vk::Pipeline pipeline = VK_NULL_HANDLE;
            [[nodiscard]] explicit operator bool() const noexcept{
                return pipeline;
            }
        };

        struct Texture
        {
            vk::ImageView image_view = VK_NULL_HANDLE;
            [[nodiscard]] explicit operator bool() const noexcept{
                return image_view;
            }
        };

        struct Mesh
        {
            vk::Buffer vertex_buffer = VK_NULL_HANDLE;
            vk::Buffer index_buffer = VK_NULL_HANDLE;
            uint32_t index_count = 0;

            [[nodiscard]] explicit operator bool() const noexcept{
                return vertex_buffer && index_buffer && index_count;
            }
        };
    }

    namespace uniforms
    {
        struct Camera
        {
            glm::mat4 view = glm::identity<glm::mat4>();
            glm::mat4 projection = glm::identity<glm::mat4>();
            glm::vec4 position = glm::vec4(0.0f);
        };

        struct Object
        {
            glm::mat4 model = glm::identity<glm::mat4>();
            glm::mat4 normals = glm::identity<glm::mat4>();
        };

        struct MaterialPhong
        {
            glm::vec4 color = glm::vec4(1.0f);
            glm::vec4 ambient = glm::vec4(0.05f);
            glm::float32 shininess = 32.0f;
            glm::float32 specular = 1.0f;
        };

        struct MaterialPbr
        {
            glm::vec4 color = glm::vec4(1.0f);
            glm::float32 roughness = 1.0f;
            glm::float32 metallic = 0.0f;
            glm::float32 ao = 1.0f;
            glm::float32 emission = 0.0f;
        };

        using Material = std::variant<MaterialPhong, MaterialPbr>;

        struct LightSettings
        {
            glm::vec4 position = glm::vec4(0.0f);
            glm::vec4 direction = glm::vec4(0.0f);
            glm::vec4 color = glm::vec4(1.0f);
            glm::mat4 space = glm::identity<glm::mat4>();
            glm::float32 quadratic = 0.1f;
            glm::float32 radius = 0.0f;
            glm::float32 intensity = 1.0f;
            glm::uint32 type = 0;
        };

        struct LightIndices
        {
            glm::uint32 count = 0;
            glm::uint32 indices[kMaxLights]{};
        };

        static_assert(sizeof(Camera) % 16 == 0, "Camera size must be multiple of 16 bytes");
    }

    struct Config
    {
        std::string app_name;                                               // Имя приложения (для драйвера Vulkan)
        std::string engine_name;                                            // Имя движка (для драйвера Vulkan)
        VulkanSurfaceProvider::Ptr surface_provider = nullptr;              // Поставщик поверхности Vulkan
        glm::vec4 clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);          // Цвет очистки
        PFN_vkGetInstanceProcAddr pfn_vk_get_proc_addr;                     // Функция получения адресов функций
        vk::Format color_format = vk::Format::eB8G8R8A8Unorm;               // Формат цветовых вложений
        vk::Format depth_format = vk::Format::eD32SfloatS8Uint;             // Формат вложений глубины и трафарета
        vk::ColorSpaceKHR color_space = vk::ColorSpaceKHR::eSrgbNonlinear;  // Цветовое пространство
        vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;        // Режим представления
        bool opengl_compatible = true;                                      // Совместимость данных с OpenGL
        bool enable_validation_layers = false;                              // Использовать слои валидации
        uint32_t max_frames_in_flight = 2;                                  // Кол-во единовременно обрабатываемых кадров
        uint32_t swap_chain_images = 3;                                     // Кол-во изображений в цепочке свопинга
    };

    struct TextureBindingInfo
    {
        TextureType type = TextureType::eAlbedoColor;
        TextureSamplerType sampler_type = TextureSamplerType::eNearest;
        handles::Texture texture = {};
    };
}