#pragma once
#include <nasral/gfx/types.h>
#include <GLFW/glfw3.h>

namespace utils
{
    struct GlfwSurfaceProvider final : nasral::gfx::VulkanSurfaceProvider
    {
        explicit GlfwSurfaceProvider(GLFWwindow* window) : window_(window)
        {
            if (!glfwVulkanSupported()){
                throw std::runtime_error("Vulkan is not supported");
            }

            uint32_t extension_count = 0;
            const char** extension_names = glfwGetRequiredInstanceExtensions(&extension_count);
            extensions_.assign(extension_names, extension_names + extension_count);
        }

        VkSurfaceKHR create_surface(const vk::Instance& instance) override
        {
            VkSurfaceKHR surface;
            glfwCreateWindowSurface(instance, window_, nullptr, &surface);
            return surface;
        }

        const std::vector<const char*>& extensions() override
        {
            return extensions_;
        }

        vk::Extent2D framebuffer_extent() override
        {
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window_, &width, &height);

            return {
                static_cast<uint32_t>(std::max(width, 0)),
                static_cast<uint32_t>(std::max(height, 0))
            };
        }

    private:
        GLFWwindow* window_;
        std::vector<const char*> extensions_;
    };
}

