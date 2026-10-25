#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>

#include <vulkan/utils/device.hpp>
#include <vulkan/utils/image.hpp>
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
        void cmd_begin_rasterization_pass();
        void cmd_end_rasterization_pass();
        void cmd_begin_screen_pass(EnumMask<FrameAttachmentType> input, EnumMask<FrameAttachmentType> output = {});
        void cmd_end_screen_pass();
        void cmd_bind_shader_program(const handles::ShaderProgram& handles);
        void cmd_bind_mesh_geometry(const handles::Mesh& handles, uint32_t uniform_idx);
        void cmd_draw_bound_geometry(uint32_t index_offset, uint32_t index_count, uint32_t mat_uniform_idx);
        void cmd_draw_screen_quad(uint32_t pass_uniform_idx);
        void cmd_gen_frame_mipmaps(const FrameAttachmentType& type) const;
        void cmd_wait_for_all() const;

        [[nodiscard]] const vk::Extent2D& rendering_resolution() const noexcept;
        [[nodiscard]] float rendering_aspect() const noexcept;
        [[nodiscard]] bool ready_for_commands() const noexcept;

        [[nodiscard]] auto is_active() const{
            return is_active_;
        }
        [[nodiscard]] auto frame_count() const{
            return frame_count_;
        }
        [[nodiscard]] auto frame_index() const{
            return frame_index_;
        }
        [[nodiscard]] const auto& vk_instance() const noexcept{
            return *vk_instance_;
        }
        [[nodiscard]] auto& vk_device() const noexcept{
            return *vk_device_;
        }
        [[nodiscard]] const auto& vk_surface() const noexcept{
            return *vk_surface_;
        }
        [[nodiscard]] const auto* vk_offscreen_image(const FrameAttachmentType& frame_type) const noexcept{
            return vk_offscreen_images_[frame_type][frame_index()].get();
        }
        [[nodiscard]] const auto& vk_swapchain_image() const noexcept{
            return vk_swapchain_images_[available_swapchain_image_];
        }
        [[nodiscard]] const auto& vk_texture_sampler(const TextureSamplerType& type) const noexcept{
            return *vk_texture_samplers_[type];
        }
        [[nodiscard]] const auto& vk_uniform_layout(const ShaderProgramType& program_type) const noexcept{
            return *vk_uniform_layouts_[program_type];
        }
        [[nodiscard]] auto& vk_uniform_buffer(const UniformBufferType& type) const noexcept{
            return *vk_uniform_buffers_[type];
        }

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
        void init_vk_swap_chain();
        void init_vk_framebuffers();
        void init_vk_uniform_layouts();
        void init_vk_texture_samplers();
        void init_vk_framebuffer_bindings();
        void init_vk_uniforms();
        void init_vk_command_buffers();
        void init_vk_synchronization();
        void refresh_vk_surface();

    private:
        // Состояние
        bool is_active_;
        bool is_frame_started_;
        std::atomic<bool> is_surface_dirty_;

        // Основные сущности Vulkan (включая кастомные RAII обертки)
        vk::UniqueInstance vk_instance_;
        VkDebugReportCallback vk_debug_callback_;
        vk::detail::DispatchLoaderDynamic vk_loader_;
        vk::UniqueSurfaceKHR vk_surface_;
        vk::utils::Device::Ptr vk_device_;
        vk::UniqueSwapchainKHR vk_swap_chain_;

        // Кадровые изображения (для показа и рендеринга)
        std::array<vk::ImageView, kMaxSwapchainImages> vk_swapchain_images_;
        EnumArray<FrameAttachmentType, std::array<vk::utils::Image::Ptr, kMaxFramesInFlight>> vk_offscreen_images_;

        // Макеты uniform-буферов для каждого типа шейдерного пайплайна
        EnumArray<ShaderProgramType, vk::utils::UniformLayout::Ptr> vk_uniform_layouts_;

        // Дескрипторные наборы для рендеринга и экранного прохода (камера, трансформации, материалы, текстуры объектов)
        EnumArray<UniformDescriptorSetType, vk::UniqueDescriptorSet> vk_rendering_d_sets_;
        EnumArray<UniformDescriptorSetType, vk::UniqueDescriptorSet> vk_screen_d_sets_;

        // Дескрипторные наборы пост-процессинга на кадр (текстуры кадровых буферов)
        std::array<vk::UniqueDescriptorSet, kMaxFramesInFlight> vk_screen_d_sets_per_frame_;

        // Uniform буферы объектов (камера, трансформации, материалы, источники света)
        EnumArray<UniformBufferType, vk::utils::Buffer::Ptr> vk_uniform_buffers_;
        // Семплеры текстур
        EnumArray<TextureSamplerType, vk::UniqueSampler> vk_texture_samplers_;

        // Синхронизация и команды (кол-во примитивов соответствует кол-ву активных кадров)
        size_t frame_count_;
        size_t frame_index_;
        uint32_t available_swapchain_image_;
        std::vector<vk::UniqueCommandBuffer> vk_command_buffers_;
        std::vector<vk::UniqueSemaphore> vk_render_available_semaphore_;
        std::vector<vk::UniqueSemaphore> vk_render_finished_semaphore_;
        std::vector<vk::UniqueFence> vk_frame_fence_;

        // Последний использованный конвейер
        vk::Pipeline vk_last_rasterization_pipeline_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::Renderer, "GFX")