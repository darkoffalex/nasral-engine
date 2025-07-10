#pragma once
#include <nasral/rendering/rendering_types.h>
#include <GLFW/glfw3.h>

namespace utils
{
    class GlfwSurfaceProvider final : public nasral::rendering::VkSurfaceProvider
    {
    public:
        explicit GlfwSurfaceProvider(GLFWwindow* window):
        VkSurfaceProvider(),
        window_(window)
        {
            // Проверка поддержки Vulkan
            if (!glfwVulkanSupported()){
                throw std::runtime_error("Vulkan is not supported");
            }

            // Получить расширения поверхности
            uint32_t extension_count = 0;
            const char** extension_names = glfwGetRequiredInstanceExtensions(&extension_count);
            extensions_.assign(extension_names, extension_names + extension_count);
        }

        VkSurfaceKHR create_surface(const vk::Instance& instance) override{
            VkSurfaceKHR surface;
            glfwCreateWindowSurface(instance, window_, nullptr, &surface);
            return surface;
        }

        std::vector<const char*>& surface_extensions() override{
            return extensions_;
        }

        ~GlfwSurfaceProvider() override = default;

    private:
        GLFWwindow* window_;
        std::vector<const char*> extensions_;
    };
}

