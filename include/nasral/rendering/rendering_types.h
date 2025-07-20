#pragma once
#include <glm/glm.hpp>
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

    class RenderingError final : public EngineError
    {
    public:
        explicit RenderingError(const std::string& message)
        : EngineError(message) {}
    };
}