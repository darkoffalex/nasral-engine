#pragma once
#include <variant>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.hpp>
#include <nasral/core_types.h>

#define MAX_CAMERAS 1
#define MAX_OBJECTS 1000
#define MAX_MATERIALS 100
#define MAX_LIGHTS 100

namespace nasral::rendering
{
    class VkSurfaceProvider
    {
    public:
        typedef std::shared_ptr<VkSurfaceProvider> Ptr;

        VkSurfaceProvider() = default;
        virtual ~VkSurfaceProvider() = default;
        virtual VkSurfaceKHR create_surface(const vk::Instance& instance) = 0;
        virtual const std::vector<const char*>& surface_extensions() = 0;
    };

    struct RenderingConfig
    {
        std::string app_name;                                               // Имя приложения
        std::string engine_name;                                            // Имя движка
        std::shared_ptr<VkSurfaceProvider> surface_provider;                // Указатель на объект получения поверхности
        std::array<float, 4> clear_color = {0.0f, 0.0f, 0.0f, 1.0f};        // Цвет очистки
        PFN_vkGetInstanceProcAddr pfn_vk_get_proc_addr;                     // Указатель на функцию получения адресов функций
        std::optional<glm::uvec2> rendering_resolution;                     // Целевое разрешение рендеринга
        vk::Format color_format = vk::Format::eB8G8R8A8Unorm;               // Формат цветовых вложений
        vk::Format depth_stencil_format = vk::Format::eD32SfloatS8Uint;     // Формат вложений глубины и трафарета
        vk::ColorSpaceKHR color_space = vk::ColorSpaceKHR::eSrgbNonlinear;  // Цветовое пространство
        vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;        // Режим представления
        bool use_opengl_style = true;                                       // Входные данные в стиле OpenGL
        bool use_validation_layers = false;                                 // Использовать слои валидации
        uint32_t max_frames_in_flight = 2;                                  // Кол-во единовременно обрабатываемых кадров
        uint32_t swap_chain_image_count = 3;                                // Кол-во изображений в цепочке свопинга
    };

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 color;
    };

    struct Handles
    {
        struct Material
        {
            vk::Pipeline pipeline = VK_NULL_HANDLE;

            [[nodiscard]] explicit operator bool() const noexcept{
                return pipeline;
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

        struct Texture
        {
            vk::ImageView image_view = VK_NULL_HANDLE;
            [[nodiscard]] explicit operator bool() const noexcept{
                return image_view;
            }
        };
    };

    enum class UniformLayoutType : unsigned
    {
        eDummy = 0,
        eBasicRasterization,
        ePostProcessing,
        TOTAL
    };

    enum class TextureSamplerType : unsigned
    {
        eNearest = 0,
        eNearestClamp,
        eLinear,
        eLinearClamp,
        eAnisotropic,
        eAnisotropicClamp,
        TOTAL
    };

    inline const std::array<std::string, static_cast<size_t>(TextureSamplerType::TOTAL)> kTextureSamplerNames = {
        "Nearest",
        "NearestClamped",
        "Linear",
        "LinearClamp",
        "Anisotropic",
        "AnisotropicClamp"
    };

    enum class TextureType : unsigned
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

    inline const std::array<std::string, static_cast<size_t>(TextureType::TOTAL)> kTextureTypePhongNames = {
        "Color",
        "Normal",
        "Specular",
        "Height",
        "Reflection"
    };

    inline const std::array<std::string, static_cast<size_t>(TextureType::TOTAL)> kTextureTypePbrNames = {
        "Albedo",
        "Normal",
        "Roughness",
        "Height",
        "Metallic"
    };

    enum class MaterialType : unsigned
    {
        eDummy = 0,
        eVertexColored,
        eTextured,
        ePhong,
        ePbr,
        TOTAL
    };

    inline const std::array<std::string, static_cast<size_t>(MaterialType::TOTAL)> kMaterialTypeNames = {
        "Dummy",
        "VertexColored",
        "Textured",
        "Phong",
        "PBR"
    };

    struct TextureBindingInfo
    {
        TextureType type = TextureType::eAlbedoColor;
        TextureSamplerType sampler_type = TextureSamplerType::eNearest;
        Handles::Texture texture = {};
    };

    struct CameraUniforms
    {
        glm::mat4 view = glm::identity<glm::mat4>();
        glm::mat4 projection = glm::identity<glm::mat4>();
        glm::vec4 position = glm::vec4(0.0f);
    };
    static_assert(sizeof(CameraUniforms) % 16 == 0, "CameraUniforms size must be multiple of 16 bytes");

    struct ObjectTransformUniforms
    {
        glm::mat4 model = glm::identity<glm::mat4>();
        glm::mat4 normals = glm::identity<glm::mat4>();
    };

    struct MaterialPhongUniforms
    {
        glm::vec4 color = glm::vec4(1.0f);
        glm::vec4 ambient = glm::vec4(0.05f);
        glm::float32 shininess = 32.0f;
        glm::float32 specular = 1.0f;
    };

    struct MaterialPbrUniforms
    {
        glm::vec4 color = glm::vec4(1.0f);
        glm::float32 roughness = 1.0f;
        glm::float32 metallic = 0.0f;
        glm::float32 ao = 1.0f;
        glm::float32 emission = 0.0f;
    };

    using MaterialUniforms = std::variant<MaterialPhongUniforms, MaterialPbrUniforms>;

    struct LightUniforms
    {
        glm::vec4 position = glm::vec4(0.0f);
        glm::vec4 direction = glm::vec4(0.0f);
        glm::vec4 color = glm::vec4(1.0f);
        glm::mat4 space = glm::identity<glm::mat4>();
        glm::float32 quadratic = 0.1f;
        glm::float32 radius = 0.0f;
        glm::float32 intensity = 1.0f;
    };

    struct LightIndices
    {
        uint32_t count = 0;
        uint32_t indices[MAX_LIGHTS]{};
    };

    class Instance
    {
    public:
        Instance() = default;
        ~Instance() = default;

        template<typename MaskType>
        void mark_changed(const MaskType& mask){
            change_mask_ |= static_cast<uint32_t>(mask);
        }

        template<typename MaskType>
        void unmark_changed(const MaskType& mask){
            change_mask_ &= ~static_cast<uint32_t>(mask);
        }

        template<typename MaskType>
        [[nodiscard]] bool check_changes(
            const MaskType& mask,
            const bool require_all = false,
            const bool unmark = false)
        {
            bool result = false;
            if(require_all){
                result = (change_mask_ & static_cast<uint32_t>(mask)) == static_cast<uint32_t>(mask);
            } else{
                result = (change_mask_ & static_cast<uint32_t>(mask)) != 0;
            }
            if(unmark) unmark_changed(mask);
            return result;
        }

    protected:
        uint32_t change_mask_ = 0;
    };

    class RenderingError final : public EngineError
    {
    public:
        explicit RenderingError(const std::string& message)
        : EngineError(message) {}
    };

    inline vk::DeviceSize size_align(const vk::DeviceSize size, const vk::DeviceSize alignment){
        return (size + alignment - 1) & ~(alignment - 1);
    }
}