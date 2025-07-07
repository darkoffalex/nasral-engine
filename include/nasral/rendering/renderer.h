#pragma once
#include <nasral/core_types.h>
#include <nasral/rendering/rendering_types.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/utils/framebuffer.hpp>

namespace nasral{class Engine;}
namespace nasral::logging{class Logger;}
namespace nasral::rendering
{
    class Renderer
    {
    public:
        typedef std::unique_ptr<Renderer> Ptr;
        typedef vk::UniqueHandle<vk::DebugReportCallbackEXT, vk::detail::DispatchLoaderDynamic> DebugReportCallback;

        Renderer(nasral::Engine* engine, const RenderingConfig& config);
        ~Renderer();

        [[nodiscard]] const SafeHandle<const Engine>& engine() const { return engine_; }
        [[nodiscard]] const RenderingConfig& config() const { return config_; }

    protected:
        [[nodiscard]] const logging::Logger* logger() const;

    private:
        SafeHandle<const Engine> engine_;
        RenderingConfig config_;

        // Состояние
        bool is_rendering_;
        std::atomic<bool> surface_refresh_required_;

        // Основные сущности Vulkan (включая кастомные RAII обертки)
        vk::Instance vk_instance_;
        DebugReportCallback vk_debug_callback_;
        vk::detail::DispatchLoaderDynamic vk_dispatch_loader_;
        vk::UniqueSurfaceKHR vk_surface_;
        vk::utils::Device::Ptr vk_device_;
        vk::UniqueRenderPass vk_render_pass_;
        vk::UniqueSwapchainKHR vk_swap_chain_;
        std::vector<vk::utils::Framebuffer::Ptr> vk_framebuffers_;

        // Синхронизация и команды
        // Кол-во примитивов соответствует кол-ву активных кадров - RenderingConfig.max_frames_in_flight
        size_t current_frame_;
        uint32_t available_image_index_;
        std::vector<vk::CommandBuffer> vk_command_buffers_;
        std::vector<vk::UniqueSemaphore> vk_render_available_semaphore_;
        std::vector<vk::UniqueSemaphore> vk_render_finished_semaphore_;
        std::vector<vk::UniqueFence> vk_frame_fence_;
    };
}