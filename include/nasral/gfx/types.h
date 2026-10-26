#pragma once

#include <memory>
#include <variant>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.hpp>
#include <nasral/common/types.h>

namespace nasral::gfx
{
    constexpr uint32_t kVkAppVersion = VK_MAKE_VERSION(1, 0, 0);
    constexpr uint32_t kVkEngineVersion = VK_MAKE_VERSION(1, 0, 0);

    constexpr uint32_t kMaxCameras = 1;
    constexpr uint32_t kMaxObjects = 1024;
    constexpr uint32_t kMaxMaterials = 128;
    constexpr uint32_t kMaxScreenPasses = 5;
    constexpr uint32_t kMaxMeshMaterials = 5;
    constexpr uint32_t kMaxLights = 64;
    constexpr uint32_t kMaxFramesInFlight = 5;
    constexpr uint32_t kMaxSwapChainImages = 5;

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 color;
    };

    typedef std::tuple<
        std::vector<Vertex>,
        std::vector<uint32_t>
    > GeometryData;

    enum class UniformRasterDSetType : uint32_t
    {
        eViewUBO = 0,
        eObjectUBOs,
        eMaterialUBOs,
        eMaterialTextures,
        eLightUBOs,
        TOTAL
    };

    enum class UniformScreenDSetType : uint32_t
    {
        eFrameTextures = 0,
        eViewUBO,
        eScreenPassUBOs,
        TOTAL
    };

    enum class UniformBufferType : uint32_t
    {
        eView = 0,
        eObjects,
        eMaterials,
        eScreenPasses,
        eLightSources,
        eLightSourcesActive,
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
        eRoughOrSpec,
        eHeight,
        eMetalOrReflect,
        eAO,
        eEmission,
        TOTAL
    };

    enum class FrameAttachmentType : uint32_t
    {
        eColor = 0,
        eGeometry,
        eAlbedo,
        eNormal,
        eDepth,
        eEmission,
        eShadows,
        eAO,
        eReflections,
        eTemp1,
        eTemp2,
        TOTAL
    };

    enum class ShaderProgramType : uint32_t
    {
        eRasterization = 0,
        eScreenPass,
        eRaytracing,
        eCompute,
        TOTAL
    };

    enum class MaterialType : uint32_t
    {
        eDummy = 0,
        eColor,
        eUnlit,
        ePhong,
        ePBR,
        TOTAL
    };

    enum class ScreenPassType : uint32_t
    {
        eCompositing,
        eEffects,
        eBlur,
        eAO,
        TOTAL
    };

    enum class PolygonMode : uint32_t
    {
        eFill = 0,
        eLine,
        ePoint,
        TOTAL
    };

    enum class LightType : uint32_t
    {
        ePointLight = 0,
        eSpotLight,
        eDirectionalLight,
        eAreaLight,
        TOTAL
    };

    enum class ViewType : uint32_t
    {
        ePerspective = 0,
        eOrthographic,
        TOTAL
    };

    struct VulkanSurfaceProvider
    {
        typedef std::shared_ptr<VulkanSurfaceProvider> Ptr;
        virtual ~VulkanSurfaceProvider() = default;
        virtual VkSurfaceKHR create_surface(const vk::Instance& instance) = 0;
        virtual const std::vector<const char*>& extensions() = 0;
        virtual vk::Extent2D framebuffer_extent() = 0;
    };

    typedef vk::UniqueHandle<vk::DebugReportCallbackEXT, vk::detail::DispatchLoaderDynamic> VkDebugReportCallback;

    namespace handles
    {
        struct ShaderProgram
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
            struct Surface
            {
                uint32_t index_offset = 0;
                uint32_t index_count = 0;
                uint32_t material_index = 0;
            };

            vk::Buffer vertex_buffer = VK_NULL_HANDLE;
            vk::Buffer index_buffer = VK_NULL_HANDLE;
            std::array<Surface, kMaxMeshMaterials> surfaces = {};
            uint32_t surfaces_count = 0;

            [[nodiscard]] explicit operator bool() const noexcept{
                return vertex_buffer && index_buffer && surfaces_count > 0;
            }
        };
    }

    namespace uniforms
    {
        struct Camera
        {
            glm::mat4 view = glm::identity<glm::mat4>();
            glm::mat4 projection = glm::identity<glm::mat4>();
            glm::mat4 view_inverse = glm::identity<glm::mat4>();
            glm::mat4 projection_inverse = glm::identity<glm::mat4>();
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

        struct ScreenPassAo
        {
            glm::uint32 type = 0;
            glm::float32 radius = 0.3f;
            glm::float32 bias = 0.02f;
            glm::float32 multiplier = 1.0f;
            glm::float32 power = 1.0f;
        };

        struct ScreenPassBlur
        {
            glm::uint32 samples = 16;
            glm::float32 base_tex_radius = 2.0f;
            glm::float32 base_kernel_radius = 6.0f;
        };

        struct ScreenPassCompositing
        {
            glm::float32 bloom_blur_lod = 2.5f;
            glm::float32 bloom_intensity = 1.0f;
            glm::float32 gamma = 2.2f;
            glm::float32 exposure = 1.0f;
        };

        using ScreenPass = std::variant<ScreenPassAo, ScreenPassBlur, ScreenPassCompositing>;

        static_assert(sizeof(Camera) % 16 == 0, "Camera size must be multiple of 16 bytes");
    }

    struct TextureBindingInfo
    {
        TextureType type = TextureType::eAlbedoColor;
        TextureSamplerType sampler_type = TextureSamplerType::eNearest;
        handles::Texture texture = {};
    };

    struct MaterialDesc
    {
        UniqueId unique_id = {};
        std::string name = {};
        MaterialType type = MaterialType::eDummy;
        std::string shader_program_path = {};
        EnumArray<TextureType, std::string> texture_paths;
        EnumArray<TextureType, TextureSamplerType> texture_samplers;

        struct
        {
            glm::vec4 color = glm::vec4(1.0f);
            glm::vec4 ambient = glm::vec4(0.05f);
            glm::float32 shininess = 32.0f;
            glm::float32 specular = 1.0f;
        } phong_settings = {};

        struct
        {
            glm::vec4 color = glm::vec4(1.0f);
            glm::float32 roughness = 1.0f;
            glm::float32 metallic = 0.0f;
            glm::float32 ao = 1.0f;
            glm::float32 emission = 0.0f;
        } pbr_settings = {};
    };

    struct ScreenPassDesc
    {
        UniqueId unique_id = {};
        std::string name = {};
        ScreenPassType type = ScreenPassType::eCompositing;
        std::optional<UniqueId> previous = {};
        std::optional<UniqueId> next = {};
        std::string shader_program_path = {};

        struct
        {
            glm::uint32 type = 0;
            glm::float32 radius = 0.3f;
            glm::float32 bias = 0.02f;
            glm::float32 multiplier = 1.0f;
            glm::float32 power = 1.0f;
        } ao_settings;

        struct
        {
            glm::uint32 samples = 16;
            glm::float32 base_tex_radius = 2.0f;
            glm::float32 base_kernel_radius = 6.0f;
        } blur_settings;

        struct
        {
            glm::float32 bloom_blur_lod = 2.5f;
            glm::float32 bloom_intensity = 1.0f;
            glm::float32 gamma = 2.2f;
            glm::float32 exposure = 1.0f;
        } compositing_settings;
    };

    struct Config
    {
        std::string app_name;                                                       // Имя приложения (для драйвера Vulkan)
        std::string engine_name;                                                    // Имя движка (для драйвера Vulkan)
        VulkanSurfaceProvider::Ptr surface_provider = nullptr;                      // Поставщик поверхности Vulkan
        glm::vec4 clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);                  // Цвет очистки
        PFN_vkGetInstanceProcAddr pfn_vk_get_proc_addr;                             // Функция получения адресов функций
        vk::Extent2D rendering_resolution = {1024, 768};                            // Разрешение кадрового буфера рендеринга
        vk::Format offscreen_color_format = vk::Format::eR16G16B16A16Sfloat;        // Формат цветовых вложений кадрового буфера рендеринга
        vk::Format offscreen_depth_format = vk::Format::eD32SfloatS8Uint;           // Формат вложений глубины и трафарета (буфер рендеринга)
        vk::Format present_color_format = vk::Format::eB8G8R8A8Unorm;               // Формат цветовых вложений презентации (показа)
        vk::ColorSpaceKHR present_color_space = vk::ColorSpaceKHR::eSrgbNonlinear;  // Цветовое пространство презентации (показа)
        vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;                // Режим представления
        vk::CompositeAlphaFlagBitsKHR composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque; // Альфа-смешивание для поверхности
        bool opengl_compatible = true;                                              // Совместимость данных с OpenGL
        bool enable_validation_layers = false;                                      // Использовать слои валидации
        uint32_t max_frames_in_flight = 2;                                          // Кол-во единовременно обрабатываемых кадров
        uint32_t swap_chain_images = 3;                                             // Кол-во изображений в цепочке свопинга
    };
}
