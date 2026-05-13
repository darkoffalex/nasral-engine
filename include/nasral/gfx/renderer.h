#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/common/index_pool.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/evt/objects/listener.h>

#include <vulkan/utils/device.hpp>
#include <vulkan/utils/framebuffer.hpp>
#include <vulkan/utils/uniform_layout.hpp>
#include <vulkan/utils/buffer.hpp>

namespace nasral::gfx
{
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

        void init();
        void finalize();

        void cmd_begin_frame();
        void cmd_end_frame();
        void cmd_bind_material(const handles::Material& handles, uint32_t uniform_idx);
        void cmd_bind_geometry(const handles::Mesh& handles, uint32_t uniform_idx);
        void cmd_draw_geometry(uint32_t index_offset, uint32_t index_count);
        void cmd_wait_for_frame() const;

        void request_surface_refresh();

        void update_cam_uniforms(const uniforms::Camera& uniforms, uint32_t index) const;
        void update_obj_uniforms(const uniforms::Object& uniforms, uint32_t index) const;
        void update_mat_phong_uniforms(const uniforms::MaterialPhong& uniforms, uint32_t index) const;
        void update_mat_pbr_uniforms(const uniforms::MaterialPbr& uniforms, uint32_t index) const;
        void update_mat_textures(const TextureBindingInfo& info, uint32_t index);
        void update_light_uniforms(const uniforms::LightSettings& uniforms, uint32_t index) const;
        void update_light_states_unsafe(const std::vector<uint32_t>& ids, bool active);
        void update_light_states(const std::vector<uint32_t>& ids, bool active);

        [[nodiscard]] auto is_active() const noexcept{ return is_active_; }
        [[nodiscard]] auto frames() const noexcept{ return frame_count_; }
        [[nodiscard]] auto frame() const noexcept{ return frame_index_; }
        [[nodiscard]] const auto& vk_instance() const noexcept{ return *vk_instance_; }
        [[nodiscard]] const auto& vk_device() const noexcept{ return *vk_device_; }
        [[nodiscard]] const auto& vk_render_pass() const noexcept{ return *vk_render_pass_; }
        [[nodiscard]] const auto& vk_surface() const noexcept{ return *vk_surface_; }
        [[nodiscard]] const auto& vk_framebuffer(const size_t index) const noexcept{ return *vk_framebuffers_[index]; }
        [[nodiscard]] const auto& vk_texture_sampler(const TextureSamplerType& type) const noexcept{ return *vk_texture_samplers_[type]; }
        [[nodiscard]] const auto& vk_uniform_layout(const UniformLayoutType& type) const noexcept{ return *vk_uniform_layouts_[type]; }
        [[nodiscard]] auto& object_ubo_ids(){ return object_ubo_ids_; }
        [[nodiscard]] auto& material_ubo_ids(){ return material_ubo_ids_; }
        [[nodiscard]] auto& light_ubo_ids(){ return light_ubo_ids_; }

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

        // Семплеры текстур
        EnumArray<TextureSamplerType, vk::UniqueSampler> vk_texture_samplers_;

        // Дескрипторные наборы (камера, трансформации и материалы объектов, текстуры объектов)
        vk::UniqueDescriptorSet vk_dset_view_;
        vk::UniqueDescriptorSet vk_dset_objects_uniforms_;
        vk::UniqueDescriptorSet vk_dset_material_uniforms_;
        vk::UniqueDescriptorSet vk_dset_material_textures_;
        vk::UniqueDescriptorSet vk_dset_light_sources_;

        // Uniform буферы объектов (камера, трансформации, материалы, источники света)
        vk::utils::Buffer::Ptr vk_ubo_view_;
        vk::utils::Buffer::Ptr vk_ubo_objects_transforms_;
        vk::utils::Buffer::Ptr vk_ubo_materials_phong_;
        vk::utils::Buffer::Ptr vk_ubo_materials_pbr_;
        vk::utils::Buffer::Ptr vk_ubo_light_sources_;
        vk::utils::Buffer::Ptr vk_ubo_light_indices_;

        // Синхронизация и команды (кол-во примитивов соответствует кол-ву активных кадров)
        size_t frame_count_;
        size_t frame_index_;
        uint32_t available_image_index_;
        std::vector<vk::UniqueCommandBuffer> vk_command_buffers_;
        std::vector<vk::UniqueSemaphore> vk_render_available_semaphore_;
        std::vector<vk::UniqueSemaphore> vk_render_finished_semaphore_;
        std::vector<vk::UniqueFence> vk_frame_fence_;

        // Индексы и пулы индексов
        IndexPool<> object_ubo_ids_;
        IndexPool<> material_ubo_ids_;
        IndexPool<> light_ubo_ids_;

        // Активные источники света (их индексы)
        std::vector<uint32_t> light_active_ids_;
        std::vector<uint8_t> light_states_;
        std::mutex light_ids_mutex_;

        // Последний использованный конвейер (pipeline)
        vk::Pipeline vk_last_pipeline_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::Renderer)