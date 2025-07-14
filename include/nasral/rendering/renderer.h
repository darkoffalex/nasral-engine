#pragma once
#include <vulkan/vulkan.hpp>
#include <nasral/core_types.h>
#include <nasral/rendering/rendering_types.h>
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

        enum class CommandGroup : size_t
        {
            eGraphicsAndPresent = 0,
            eTransfer,
            TOTAL
        };

        Renderer(const Engine* engine, RenderingConfig config);
        ~Renderer();

        void cmd_begin_frame();
        void cmd_end_frame();
        void cmd_wait_for_frame() const;
        void request_surface_refresh();

        [[nodiscard]] const SafeHandle<const Engine>& engine() const { return engine_; }
        [[nodiscard]] const RenderingConfig& config() const { return config_; }
        [[nodiscard]] const vk::Instance& vk_instance() const { return *vk_instance_; }
        [[nodiscard]] const vk::utils::Device::Ptr& vk_device() const { return vk_device_; }

        static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_report_callback(
            vk::Flags<vk::DebugReportFlagBitsEXT> flags,
            vk::DebugReportObjectTypeEXT object_type,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char* layer_prefix,
            const char* msg,
            void* user_data);

    private:
        [[nodiscard]] const logging::Logger* logger() const;

        void init_vk_instance();
        void init_vk_loader();
        void init_vk_debug_callback();
        void init_vk_surface();
        void init_vk_device();
        void init_vk_render_passes();
        void init_vk_swap_chain();
        void init_vk_framebuffers();
        void init_vk_command_buffers();
        void init_vk_sync_objects();
        void refresh_vk_surface();

    protected:
        SafeHandle<const Engine> engine_;
        RenderingConfig config_;

        // Состояние
        bool is_rendering_;
        std::atomic<bool> surface_refresh_required_;

        // Основные сущности Vulkan (включая кастомные RAII обертки)
        vk::UniqueInstance vk_instance_;
        DebugReportCallback vk_debug_callback_;
        vk::detail::DispatchLoaderDynamic vk_dispatch_loader_;
        vk::UniqueSurfaceKHR vk_surface_;
        vk::utils::Device::Ptr vk_device_;
        vk::UniqueRenderPass vk_render_pass_;
        vk::UniqueSwapchainKHR vk_swap_chain_;
        std::vector<vk::utils::Framebuffer::Ptr> vk_framebuffers_;

        // Синхронизация и команды (кол-во примитивов соответствует кол-ву активных кадров)
        size_t current_frame_;
        uint32_t available_image_index_;
        std::vector<vk::UniqueCommandBuffer> vk_command_buffers_;
        std::vector<vk::UniqueSemaphore> vk_render_available_semaphore_;
        std::vector<vk::UniqueSemaphore> vk_render_finished_semaphore_;
        std::vector<vk::UniqueFence> vk_frame_fence_;
    };
}