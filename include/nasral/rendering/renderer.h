#pragma once
#include <vulkan/vulkan.hpp>
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
        void cmd_bind_material(const Handles::Material& handles);
        void cmd_bind_frame_descriptors();
        void cmd_draw_mesh(const Handles::Mesh& handles, uint32_t obj_index);
        void cmd_wait_for_frame() const;

        void request_surface_refresh();
        void update_cam_ubo(uint32_t index, const CameraUniforms& uniforms) const;
        void update_obj_ubo(uint32_t index, const ObjectTransformUniforms& uniforms) const;
        void update_obj_ubo(uint32_t index, const ObjectPhongMatUniforms& uniforms) const;
        void update_obj_ubo(uint32_t index, const ObjectPbrMatUniforms& uniforms) const;
        void update_obj_tex(uint32_t index, const Handles::Texture& handles, const TextureType& t_type, const TextureSamplerType& s_type);
        void update_light_ubo(uint32_t index, const LightUniforms& uniforms) const;

        [[nodiscard]] uint32_t obj_id_acquire_unsafe();
        [[nodiscard]] uint32_t obj_id_acquire();
        void obj_id_release_unsafe(uint32_t id);
        void obj_id_release(uint32_t id);
        void obj_ids_reset_unsafe();
        void obj_ids_reset();

        [[nodiscard]] uint32_t light_id_acquire_unsafe();
        [[nodiscard]] uint32_t light_id_acquire();
        void light_id_release_unsafe(uint32_t id);
        void light_id_release(uint32_t id);
        void light_ids_reset_unsafe();
        void light_ids_reset();
        void light_ids_activate_unsafe(const std::vector<uint32_t>& ids);
        void light_ids_activate(const std::vector<uint32_t>& ids);
        void light_ids_deactivate_unsafe(const std::vector<uint32_t>& ids);
        void light_ids_deactivate(const std::vector<uint32_t>& ids);

        [[nodiscard]] bool is_rendering() const { return is_rendering_; }
        [[nodiscard]] size_t current_frame() const { return current_frame_; }
        [[nodiscard]] const SafeHandle<const Engine>& engine() const { return engine_; }
        [[nodiscard]] const RenderingConfig& config() const { return config_; }
        [[nodiscard]] const vk::Instance& vk_instance() const { return *vk_instance_; }
        [[nodiscard]] const vk::utils::Device::Ptr& vk_device() const { return vk_device_; }
        [[nodiscard]] const vk::RenderPass& vk_render_pass() const { return *vk_render_pass_; }
        [[nodiscard]] const vk::SurfaceKHR& vk_surface() const { return *vk_surface_; }
        [[nodiscard]] const vk::utils::Framebuffer& vk_framebuffer(const size_t index) const { return *vk_framebuffers_[index];}
        [[nodiscard]] const vk::Sampler& vk_texture_sampler(const TextureSamplerType& type) const { return *vk_texture_samplers_[to<size_t>(type)];}
        [[nodiscard]] const vk::utils::UniformLayout& vk_uniform_layout(const UniformLayoutType& type) const { return *vk_uniform_layouts_[to<size_t>(type)];}

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
        void init_vk_texture_samplers();
        void init_vk_uniforms();
        void init_vk_command_buffers();
        void init_vk_sync_objects();
        void init_index_pools();
        void refresh_vk_surface();

        template<typename T>
        [[nodiscard]] vk::DeviceSize aligned_ubo_size() const{
            assert(vk_device_);
            auto& d = vk_device_->physical_device();
            static const auto alignment = d.getProperties().limits.minUniformBufferOffsetAlignment;
            return size_align(sizeof(T), alignment);
        }

        template<typename T>
        [[nodiscard]] vk::DeviceSize aligned_sbo_size() const{
            assert(vk_device_);
            auto& d = vk_device_->physical_device();
            static const auto alignment = d.getProperties().limits.minStorageBufferOffsetAlignment;
            return size_align(sizeof(T), alignment);
        }

        template<typename T>
        [[nodiscard]] vk::DeviceSize ubo_offset(const uint32_t index) const{
            assert(vk_device_);
            return aligned_ubo_size<T>() * index;
        }

        template<typename T>
        [[nodiscard]] vk::DeviceSize sbo_offset(const uint32_t index) const{
            assert(vk_device_);
            return aligned_sbo_size<T>() * index;
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

        // Макеты конвейеров
        std::vector<vk::utils::UniformLayout::Ptr> vk_uniform_layouts_;
        // Семплеры текстур
        std::array<vk::UniqueSampler, static_cast<size_t>(TextureSamplerType::TOTAL)> vk_texture_samplers_;
        // Дескрипторные наборы (камера, трансформации и материалы объектов, текстуры объектов)
        vk::UniqueDescriptorSet vk_dset_view_;
        vk::UniqueDescriptorSet vk_dset_objects_uniforms_;
        vk::UniqueDescriptorSet vk_dset_objects_textures_;
        vk::UniqueDescriptorSet vk_dset_light_sources_;
        // Uniform буферы объектов (камера, трансформации, материалы, источники света)
        vk::utils::Buffer::Ptr vk_ubo_view_;
        vk::utils::Buffer::Ptr vk_ubo_objects_transforms_;
        vk::utils::Buffer::Ptr vk_ubo_objects_phong_mat_;
        vk::utils::Buffer::Ptr vk_ubo_objects_pbr_mat_;
        vk::utils::Buffer::Ptr vk_ubo_light_sources_;
        vk::utils::Buffer::Ptr vk_ubo_light_indices_;

        // Синхронизация и команды (кол-во примитивов соответствует кол-ву активных кадров)
        size_t current_frame_;
        uint32_t available_image_index_;
        std::vector<vk::UniqueCommandBuffer> vk_command_buffers_;
        std::vector<vk::UniqueSemaphore> vk_render_available_semaphore_;
        std::vector<vk::UniqueSemaphore> vk_render_finished_semaphore_;
        std::vector<vk::UniqueFence> vk_frame_fence_;

        // Индексы объектов
        std::vector<uint32_t> object_ids_;
        std::mutex obj_ids_mutex_;

        // Индексы источников света
        std::vector<uint32_t> light_ids_;
        std::vector<uint32_t> active_light_ids_;
        std::mutex light_ids_mutex_;
    };
}