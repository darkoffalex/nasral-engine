#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.hpp>
#include <nasral/core_types.h>

#define MAX_CAMERAS 1
#define MAX_OBJECTS 1000
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
            vk::Image image = VK_NULL_HANDLE;
            vk::ImageView image_view = VK_NULL_HANDLE;

            [[nodiscard]] explicit operator bool() const noexcept{
                return image && image_view;
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

    struct CameraUniforms
    {
        glm::mat4 view = glm::identity<glm::mat4>();
        glm::mat4 projection = glm::identity<glm::mat4>();
    };

    struct ObjectUniforms
    {
        glm::mat4 model = glm::identity<glm::mat4>();
    };

    static_assert(sizeof(CameraUniforms) % 16 == 0, "CameraUniforms size must be multiple of 16 bytes");
    static_assert(sizeof(ObjectUniforms) % 16 == 0, "ObjectUniforms size must be multiple of 16 bytes");

    class RenderingError final : public EngineError
    {
    public:
        explicit RenderingError(const std::string& message)
        : EngineError(message) {}
    };

    inline uint32_t size_align(const uint32_t size, const uint32_t alignment){
        return (size + alignment - 1) & ~(alignment - 1);
    }
}