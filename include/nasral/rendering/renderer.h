#pragma once
#include <vulkan/vulkan.hpp>
#include <nasral/core_types.h>
#include <nasral/rendering/rendering_types.h>
#include <vulkan/utils/framebuffer.hpp>
#include <vulkan/utils/buffer.hpp>
#include <vulkan/utils/uniform_layout.hpp>

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
        void cmd_bind_material_pipeline(const Handles::Material& handles);
        void cmd_bind_cam_descriptors();
        void cmd_bind_obj_descriptors(uint32_t obj_index);
        void cmd_draw_mesh(const Handles::Mesh& handles);
        void cmd_wait_for_frame() const;
        void request_surface_refresh();
        void update_cam_uniforms(const CameraUniforms& uniforms, size_t index = 0);
        void update_obj_uniforms(const ObjectUniforms& uniforms, size_t index = 0);

        [[nodiscard]] bool is_rendering() const { return is_rendering_; }
        [[nodiscard]] size_t current_frame() const { return current_frame_; }
        [[nodiscard]] const SafeHandle<const Engine>& engine() const { return engine_; }
        [[nodiscard]] const RenderingConfig& config() const { return config_; }
        [[nodiscard]] const vk::Instance& vk_instance() const { return *vk_instance_; }
        [[nodiscard]] const vk::utils::Device::Ptr& vk_device() const { return vk_device_; }
        [[nodiscard]] const vk::RenderPass& vk_render_pass() const { return *vk_render_pass_; }
        [[nodiscard]] const vk::SurfaceKHR& vk_surface() const { return *vk_surface_; }
        [[nodiscard]] const vk::utils::Framebuffer& vk_framebuffer(const size_t index) const { return *vk_framebuffers_[index]; }
        [[nodiscard]] const vk::utils::UniformLayout& vk_uniform_layout(const size_t index) const { return *vk_uniform_layouts_[index]; }

        [[nodiscard]] vk::Extent2D get_rendering_resolution() const;
        [[nodiscard]] float get_rendering_aspect() const;

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
        void init_vk_uniform_layouts();
        void init_vk_uniforms();
        void init_vk_command_buffers();
        void init_vk_sync_objects();
        void refresh_vk_surface();

        template<typename T>
        void update_uniforms(vk::utils::Buffer& buffer, const uint32_t index, const T& data) {
            assert(vk_device_);
            const auto alignment = vk_device_->physical_device().getProperties().limits.minUniformBufferOffsetAlignment;
            const auto offset = size_align(sizeof(T), alignment) * index;
            auto* ptr = buffer.map(offset, sizeof(T));
            std::memcpy(ptr, &data, sizeof(T));
            buffer.unmap();
        }

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

        // Uniform-буферы и дескрипторы
        std::vector<vk::utils::UniformLayout::Ptr> vk_uniform_layouts_;
        vk::UniqueDescriptorSet vk_dset_view_;
        vk::UniqueDescriptorSet vk_dset_objects_;
        vk::utils::Buffer::Ptr vk_ubo_view_;
        vk::utils::Buffer::Ptr vk_ubo_objects_;

        // Синхронизация и команды (кол-во примитивов соответствует кол-ву активных кадров)
        size_t current_frame_;
        uint32_t available_image_index_;
        std::vector<vk::UniqueCommandBuffer> vk_command_buffers_;
        std::vector<vk::UniqueSemaphore> vk_render_available_semaphore_;
        std::vector<vk::UniqueSemaphore> vk_render_finished_semaphore_;
        std::vector<vk::UniqueFence> vk_frame_fence_;
    };
}