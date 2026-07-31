#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>

#include <vulkan/utils/device.hpp>
#include <vulkan/utils/framebuffer.hpp>
#include <vulkan/utils/uniform_layout.hpp>
#include <vulkan/utils/buffer.hpp>

namespace nasral::gfx
{
    class Manager;
    class Renderer final : public Subsystem<Renderer, Config>, public log::Loggable<Renderer>
    {
    public:
        typedef std::unique_ptr<Renderer> Ptr;

        enum class CmdGroupType
        {
            eGraphicsAndPresent = 0,
            eTransfer,
            TOTAL
        };

        explicit Renderer(Engine* e, const Config& config);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        void cmd_begin_frame();
        void cmd_end_frame();
        void cmd_bind_material(const handles::Material& handles, uint32_t uniform_idx);
        void cmd_bind_geometry(const handles::Mesh& handles, uint32_t uniform_idx);
        void cmd_draw_geometry(uint32_t index_offset, uint32_t index_count);
        void cmd_wait_for_all() const;

        void request_surface_refresh();

        [[nodiscard]] auto is_active() const noexcept{ return is_active_; }
        [[nodiscard]] auto frames() const noexcept{ return frame_count_; }
        [[nodiscard]] auto frame() const noexcept{ return frame_index_; }
        [[nodiscard]] const auto& vk_instance() const noexcept{ return *vk_instance_; }
        [[nodiscard]] auto& vk_device() const noexcept{ return *vk_device_; }
        [[nodiscard]] const auto& vk_render_pass() const noexcept{ return *vk_render_pass_; }
        [[nodiscard]] const auto& vk_surface() const noexcept{ return *vk_surface_; }
        [[nodiscard]] const auto& vk_framebuffer(const size_t index) const noexcept{ return *vk_framebuffers_[index]; }
        [[nodiscard]] const auto& vk_texture_sampler(const TextureSamplerType& type) const noexcept{ return *vk_texture_samplers_[type]; }
        [[nodiscard]] const auto& vk_uniform_layout(const UniformLayoutType& type) const noexcept{ return *vk_uniform_layouts_[type]; }
        [[nodiscard]] const auto& vk_uniform_d_set(const UniformDSetType& type) const noexcept{ return *vk_descriptor_sets_[type]; }
        [[nodiscard]] auto& vk_uniform_buffer(const UniformBufferType& type) const noexcept{ return *vk_uniform_buffers_[type]; }

        [[nodiscard]] const vk::Extent2D& rendering_resolution() const noexcept;
        [[nodiscard]] float rendering_aspect() const noexcept;
        [[nodiscard]] bool ready_for_commands() const noexcept;

        static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_report_callback(
            vk::Flags<vk::DebugReportFlagBitsEXT> flags,
            vk::DebugReportObjectTypeEXT object_type,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char* layer_prefix,
            const char* msg,
            void* user_data);

    protected:
        void init_vk_instance();
        void init_vk_loader();
        void init_vk_debug_callback();
        void init_vk_surface();
        void init_vk_device();
        void init_vk_render_passes();
        void init_vk_swap_chain();
        void init_vk_framebuffers();
        void init_vk_uniform_layouts();
        void init_vk_texture_samplers();
        void init_vk_uniforms();
        void init_vk_command_buffers();
        void init_vk_synchronization();
        void refresh_vk_surface();

    private:
        // Состояние
        bool is_active_;
        bool frame_in_progress_;
        std::atomic<bool> surface_refresh_needed_;

        // Основные сущности Vulkan (включая кастомные RAII обертки)
        vk::UniqueInstance vk_instance_;
        VkDebugReportCallback vk_debug_callback_;
        vk::detail::DispatchLoaderDynamic vk_loader_;
        vk::UniqueSurfaceKHR vk_surface_;
        vk::utils::Device::Ptr vk_device_;
        vk::UniqueRenderPass vk_render_pass_;
        vk::UniqueSwapchainKHR vk_swap_chain_;
        std::vector<vk::utils::Framebuffer::Ptr> vk_framebuffers_;

        // Макеты конвейеров (для растеризации, пост-процессинга и прочего)
        EnumArray<UniformLayoutType, vk::utils::UniformLayout::Ptr> vk_uniform_layouts_;
        // Дескрипторные наборы (камера, трансформации и материалы объектов, текстуры объектов)
        EnumArray<UniformDSetType, vk::UniqueDescriptorSet> vk_descriptor_sets_;
        // Uniform буферы объектов (камера, трансформации, материалы, источники света)
        EnumArray<UniformBufferType, vk::utils::Buffer::Ptr> vk_uniform_buffers_;
        // Семплеры текстур
        EnumArray<TextureSamplerType, vk::UniqueSampler> vk_texture_samplers_;

        // Синхронизация и команды (кол-во примитивов соответствует кол-ву активных кадров)
        size_t frame_count_;
        size_t frame_index_;
        uint32_t available_image_index_;
        std::vector<vk::UniqueCommandBuffer> vk_command_buffers_;
        std::vector<vk::UniqueSemaphore> vk_render_available_semaphore_;
        std::vector<vk::UniqueSemaphore> vk_render_finished_semaphore_;
        std::vector<vk::UniqueFence> vk_frame_fence_;

        // Последний использованный конвейер (pipeline)
        vk::Pipeline vk_last_pipeline_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::Renderer, "GFX")
