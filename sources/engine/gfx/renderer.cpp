#include "pch.h"
#include <nasral/gfx/renderer.h>
#include <nasral/engine.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/utils.h>

namespace nasral::gfx
{
    VkBool32 Renderer::vk_debug_report_callback(
        [[maybe_unused]] vk::Flags<vk::DebugReportFlagBitsEXT> flags,
        [[maybe_unused]] vk::DebugReportObjectTypeEXT object_type,
        [[maybe_unused]] uint64_t obj,
        [[maybe_unused]] size_t location,
        [[maybe_unused]] int32_t code,
        const char* layer_prefix,
        const char* msg,
        void* user_data)
    {
        auto* logger = static_cast<log::Logger*>(user_data);
        const std::string msg_str = "Vulkan validation: " + std::string(layer_prefix) + " | " + std::string(msg) + "\n";
        logger->warn(msg_str);
        return VK_FALSE;
    }

    Renderer::Renderer(Engine* e, const Config& config)
        : Subsystem(e, config)
        , is_active_(false)
        , frame_in_progress_(false)
        , surface_refresh_needed_(false)
        , frame_count_(0)
        , frame_index_(0)
        , available_image_index_(0)
        , vk_last_rasterization_pipeline_(VK_NULL_HANDLE)
    {
        log_info("Initializing Vulkan renderer...");

        init_vk_instance();
        log_info("Vulkan: Instance initialized.");

        init_vk_loader();
        log_info("Vulkan: Loader initialized.");

        init_vk_debug_callback();
        log_info("Vulkan: Debug callback initialized.");

        init_vk_surface();
        log_info("Vulkan: Surface initialized.");

        init_vk_device();
        const std::string device_name = vk_device_->physical_device().getProperties().deviceName;
        log_info("Vulkan: Device initialized (" + device_name + ").");

        init_vk_render_passes();
        log_info("Vulkan: Render passes initialized.");

        init_vk_swap_chain();
        log_info("Vulkan: Swap chain initialized.");

        init_vk_framebuffers();
        const auto extent_render = vk_offscreen_framebuffers_[0]->extent();
        const auto extent_swapchain = vk_swapchain_framebuffers_[0]->extent();
        const std::string extent_render_str = std::to_string(extent_render.width) + "x" + std::to_string(extent_render.height);
        const std::string extent_swapchain_str = std::to_string(extent_swapchain.width) + "x" + std::to_string(extent_swapchain.height);
        log_info("Vulkan: Framebuffers initialized (" + extent_render_str + " -> " + extent_swapchain_str + ").");

        init_vk_uniform_layouts();
        log_info("Vulkan: Uniform layouts initialized.");

        init_vk_texture_samplers();
        log_info("Vulkan: Texture samplers initialized.");

        init_vk_uniforms();
        log_info("Vulkan: Uniforms initialized.");

        init_vk_framebuffer_bindings();
        log_info("Vulkan: Framebuffer attachments bound to post-processing descriptor sets.");

        init_vk_command_buffers();
        log_info("Vulkan: Command buffers initialized.");

        init_vk_synchronization();
        log_info("Vulkan: Synchronization initialized.");

        is_active_ = true;

        log_info("Renderer initialized.");
    }

    Renderer::~Renderer()
    {
        is_active_ = false;
        cmd_wait_for_all();
        log_info("Renderer destroyed");
    }

#pragma region render_commands

    void Renderer::cmd_begin_frame()
    {
        if (surface_refresh_needed_.exchange(false, std::memory_order_acquire)){
            refresh_vk_surface();
        }
        if (!is_active()) return;

        assert(!frame_in_progress_ && "Frame already in progress");
        frame_in_progress_ = true;
        vk_last_rasterization_pipeline_ = VK_NULL_HANDLE;

        // Ожидание забора кадра
        (void)vk_device_->logical_device().waitForFences(1u, &vk_frame_fence_[frame()].get(), VK_TRUE, std::numeric_limits<uint64_t>::max());
        (void)vk_device_->logical_device().resetFences(1u, &vk_frame_fence_[frame()].get());

        // Получить свободный Swapchain image
        const auto result = vk_device_->logical_device().acquireNextImageKHR(
            vk_swap_chain_.get(),
            std::numeric_limits<uint64_t>::max(),
            vk_render_available_semaphore_[frame()].get(),
            VK_NULL_HANDLE,
            &available_image_index_);

        if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR) {
            request_surface_refresh();
            return;
        }

        // Открываем командный буфер для записи
        auto& cmd_buffer = vk_command_buffers_[frame()];
        cmd_buffer->reset();
        cmd_buffer->begin(vk::CommandBufferBeginInfo());
    }

    void Renderer::cmd_end_frame()
    {
        if (!ready_for_commands()) return;

        auto& cmd_buffer = vk_command_buffers_[frame()];
        cmd_buffer->end(); // Закрываем командный буфер

        // Один submit отправляет ВСЕ команды на GPU
        std::array<vk::Semaphore, 1> wait_semaphores{ vk_render_available_semaphore_[frame()].get() };
        std::array<vk::Semaphore, 1> signal_semaphores{ vk_render_finished_semaphore_[available_image_index_].get() };
        std::array<vk::PipelineStageFlags, 1> wait_stages{ vk::PipelineStageFlagBits::eColorAttachmentOutput };

        const auto& group = vk_device_->queue_group(static_cast<size_t>(CmdGroupType::eGraphicsAndPresent));
        auto& queue = group.queues[0];

        queue.submit(vk::SubmitInfo()
            .setCommandBuffers(cmd_buffer.get())
            .setWaitSemaphores(wait_semaphores)
            .setWaitDstStageMask(wait_stages)
            .setSignalSemaphores(signal_semaphores),
            vk_frame_fence_[frame()].get());

        try {
            (void)queue.presentKHR(vk::PresentInfoKHR()
                .setSwapchains(vk_swap_chain_.get())
                .setWaitSemaphores(signal_semaphores)
                .setImageIndices(available_image_index_));
        }
        catch (const ::vk::OutOfDateKHRError&) {
            request_surface_refresh();
        }

        ++frame_count_;
        frame_index_ = frame_count_ % static_cast<size_t>(config().max_frames_in_flight);
        frame_in_progress_ = false;
    }

    void Renderer::cmd_begin_rasterization_pass()
    {
        if (!ready_for_commands()){
            return;
        }

        auto& cmd_buffer = vk_command_buffers_[frame()];
        const auto& extent = vk_offscreen_framebuffers_[frame()]->extent();

        // Цвет и глубина/трафарет очистки
        std::array<vk::ClearValue, 4> clear_values{};
        clear_values[0].color = vk::ClearColorValue(config().clear_color.r, config().clear_color.g, config().clear_color.b, config().clear_color.a);
        clear_values[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
        clear_values[2].color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        clear_values[3].color = vk::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f);

        // Команда начала прохода
        cmd_buffer->beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(vk_rasterization_pass_.get())
                .setFramebuffer(vk_offscreen_framebuffers_[frame()]->vk_framebuffer())
                .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), extent))
                .setClearValues(clear_values),
            vk::SubpassContents::eInline);

        // Привязать UBO дескрипторы для 3D геометрии
        const auto& pipeline_l = vk_uniform_layouts_[UniformLayoutType::eRasterization]->vk_pipeline_layout();
        cmd_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_l, 0,
            {
                vk_rasterization_d_sets_[UniformDSetType::eViewUBO].get(),
                vk_rasterization_d_sets_[UniformDSetType::eObjectUBOs].get(),
                vk_rasterization_d_sets_[UniformDSetType::eMaterialUBOs].get(),
                vk_rasterization_d_sets_[UniformDSetType::eMaterialTextures].get(),
                vk_rasterization_d_sets_[UniformDSetType::eLightUBOs].get()
            }, {});
    }

    void Renderer::cmd_begin_post_processing_pass()
    {
        if (!ready_for_commands()){
            return;
        }

        // Получить командный буфер ТЕКУЩЕГО КАДРА
        auto& cmd_buffer = vk_command_buffers_[frame()];
        // Размер ДОСТУПНОГО ИЗОБРАЖЕНИЯ swap chain
        const auto& extent = vk_swapchain_framebuffers_[available_image_index_]->extent();

        // Очистка (глубина не нужна)
        std::array<vk::ClearValue, 1> clear_values{};
        clear_values[0].color = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);

        // Команда начала прохода
        cmd_buffer->beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(vk_post_processing_pass_.get())
                .setFramebuffer(vk_swapchain_framebuffers_[available_image_index_]->vk_framebuffer())
                .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), extent))
                .setClearValues(clear_values),
            vk::SubpassContents::eInline);

        // Привязать дескрипторы текстур кадрового буфера ДЛЯ ТЕКУЩЕГО КАДРА
        const auto& pipeline_l = vk_uniform_layouts_[UniformLayoutType::ePostProcessing]->vk_pipeline_layout();
        cmd_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_l, 0,
            {
                vk_post_process_d_sets_[frame()].get()
            }, {});
    }

    void Renderer::cmd_end_render_pass()
    {
        if (!ready_for_commands()){
            return;
        }

        vk_command_buffers_[frame()]->endRenderPass();
    }

    void Renderer::cmd_bind_rasterization_material(const handles::Material& handles, const uint32_t uniform_idx)
    {
        if (!ready_for_commands()){
            return;
        }

        // Размеры области рендеринга
        const auto& extent = rendering_resolution();
        const auto& width = extent.width;
        const auto& height = extent.height;

        // Область вида (динамическое состояние конвейера)
        auto viewport = vk::Viewport()
            .setX(0.0f)
            .setWidth(static_cast<float>(width))
            .setMinDepth(0.0f)
            .setMaxDepth(1.0f);

        // Совместимость координат с OpenGL
        if (config().opengl_compatible){
            viewport.setY(static_cast<float>(height));
            viewport.setHeight(-static_cast<float>(height));
        }else{
            viewport.setY(0.0f);
            viewport.setHeight(static_cast<float>(height));
        }

        // Ножницы (динамическое состояние конвейера)
        auto scissor = vk::Rect2D()
            .setOffset(vk::Offset2D(0, 0))
            .setExtent(extent);

        // Получить макет конвейера
        const auto& pipeline_l = vk_uniform_layouts_[UniformLayoutType::eRasterization]->vk_pipeline_layout();

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame()];

        // Передать индекс uniform параметров материала через push constant
        cmd_buffer->pushConstants(
            pipeline_l,
            vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(uint32_t),
            &uniform_idx);

        /* Запись команд */

        // Если конвейер сменился - привязать
        if (vk_last_rasterization_pipeline_ != handles.pipeline){
            cmd_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, handles.pipeline);
        }

        // Обновить последний привязанный конвейер
        vk_last_rasterization_pipeline_ = handles.pipeline;

        // Запись команд. Привязать динамические состояния
        cmd_buffer->setViewport(0, {viewport});
        cmd_buffer->setScissor(0, {scissor});
    }

    void Renderer::cmd_bind_post_processing_material(const handles::Material& handles)
    {
        if (!ready_for_commands()){
            return;
        }

        // Получить командный буфер ТЕКУЩЕГО КАДРА
        auto& cmd_buffer = vk_command_buffers_[frame()];

        // Размер ДОСТУПНОГО ИЗОБРАЖЕНИЯ swap chain
        const auto& extent = vk_swapchain_framebuffers_[available_image_index_]->extent();
        const auto& width = extent.width;
        const auto& height = extent.height;

        // Область вида (динамическое состояние конвейера)
        auto viewport = vk::Viewport()
            .setX(0.0f)
            .setWidth(static_cast<float>(width))
            .setMinDepth(0.0f)
            .setMaxDepth(1.0f);

        // Совместимость координат с OpenGL
        if (config().opengl_compatible){
            viewport.setY(static_cast<float>(height));
            viewport.setHeight(-static_cast<float>(height));
        }else{
            viewport.setY(0.0f);
            viewport.setHeight(static_cast<float>(height));
        }

        // Ножницы (динамическое состояние конвейера)
        auto scissor = vk::Rect2D()
            .setOffset(vk::Offset2D(0, 0))
            .setExtent(extent);

        // Привязать конвейер и динамические состояния
        cmd_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, handles.pipeline);
        cmd_buffer->setViewport(0, {viewport});
        cmd_buffer->setScissor(0, {scissor});
    }

    void Renderer::cmd_bind_rasterization_geometry(const handles::Mesh& handles, const uint32_t uniform_idx)
    {
        if (!ready_for_commands()){
            return;
        }

        // Получить макет конвейера
        const auto& pipeline_l = vk_uniform_layouts_[UniformLayoutType::eRasterization]->vk_pipeline_layout();

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame()];

        // Передать индекс uniform объекта через push constant
        cmd_buffer->pushConstants(
            pipeline_l,
            vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment,
            sizeof(uint32_t),
            sizeof(uint32_t),
            &uniform_idx);

        // Запись команд. Привязать геометрию и нарисовать её
        cmd_buffer->bindVertexBuffers(0, {handles.vertex_buffer}, {0});
        cmd_buffer->bindIndexBuffer(handles.index_buffer, 0, vk::IndexType::eUint32);
    }

    void Renderer::cmd_draw_geometry(const uint32_t index_offset, const uint32_t index_count)
    {
        if (!ready_for_commands()){
            return;
        }

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame()];

        cmd_buffer->drawIndexed(index_count, 1, index_offset, 0, 0);
    }

    void Renderer::cmd_draw_post_processing_quad()
    {
        if (!ready_for_commands()){
            return;
        }

        auto& cmd_buffer = vk_command_buffers_[frame()];
        cmd_buffer->draw(6, 1, 0, 0);
    }

    void Renderer::cmd_gen_framebuffer_mipmaps(const OffscreenTextureType& type) const
    {
        if (!ready_for_commands()){
            return;
        }

        const auto& cmd_buffer = vk_command_buffers_[frame()];
        const auto& framebuffer = vk_offscreen_framebuffers_[frame()];
        const auto& attachment = framebuffer->attachments()[static_cast<size_t>(type)];
        const auto& extent = framebuffer->extent();

        attachment->write_gen_mipmaps_commands(cmd_buffer.get()
            , {extent.width, extent.height, 1}
            , vk::ImageLayout::eShaderReadOnlyOptimal
            , vk::AccessFlagBits::eShaderWrite
            , vk::PipelineStageFlagBits::eFragmentShader
            , vk::ImageAspectFlagBits::eColor);
    }

    void Renderer::cmd_wait_for_all() const
    {
        vk_device_->logical_device().waitIdle();
    }

    void Renderer::request_surface_refresh()
    {
        surface_refresh_needed_.store(true, std::memory_order_release);
    }

#pragma endregion

    /**
     * @brief Обновляет (пересоздаёт) объекты, зависящие от поверхности.
     * @details Вызывается при изменении размеров/параметров поверхности (resize, смена DPI, alt‑tab и т.п.).
     * Выполняет полную реконфигурацию: ждём завершения GPU (waitIdle), временно отключаем рендеринг и сбрасываем
     * индекс кадра; очищаем командные буферы и уничтожаем кадровые буферы; пересоздаём swap‑chain (с учётом oldSwapchain),
     * затем создаём новые framebuffer’ы под его изображения и заново выделяем командные буферы для актуальных кадровых
     * буферов. Командные буферы выделяются заново, так как старые могли содержать команды, ссылающиеся на объекты
     * старого swap‑chain (изображения, размеры, макеты), что недопустимо. В конце рендеринг снова включается.
     */
    void Renderer::refresh_vk_surface()
    {
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_surface_ && "Vulkan surface is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

        // Ожидать завершения всех команд
        cmd_wait_for_all();

        // Отключить рендеринг и сбросить кадр
        is_active_ = false;
        frame_count_ = 0;
        frame_index_ = 0;

        // Очистить командные буферы
        vk_command_buffers_.clear();
        log_info("Vulkan: command buffers cleared.");

        // Уничтожить кадровые буферы
        vk_swapchain_framebuffers_.clear();
        vk_offscreen_framebuffers_.clear();
        log_info("Vulkan: Framebuffers cleared.");

        // Пере-создать swap-chain (старый будет задействован при создании нового, затем удален)
        init_vk_swap_chain();
        log_info("Vulkan: swap-chain recreated.");

        // Создать новые кадровые буферы
        init_vk_framebuffers();
        const auto extent_render = vk_offscreen_framebuffers_[0]->extent();
        const auto extent_swapchain = vk_swapchain_framebuffers_[0]->extent();
        const std::string extent_render_str = std::to_string(extent_render.width) + "x" + std::to_string(extent_render.height);
        const std::string extent_swapchain_str = std::to_string(extent_swapchain.width) + "x" + std::to_string(extent_swapchain.height);
        log_info("Vulkan: Framebuffers initialized (" + extent_render_str + " -> " + extent_swapchain_str + ").");

        // Создать новые командные буферы
        init_vk_command_buffers();
        log_info("Vulkan: command buffers recreated.");

        // Связать кадровые (offscreen) буферами с дескрипторами пост-процессинга
        init_vk_framebuffer_bindings();

        // Включить рендеринг
        is_active_ = true;
    }

    const vk::Extent2D& Renderer::rendering_resolution() const noexcept{
        return config().rendering_resolution;
    }

    float Renderer::rendering_aspect() const noexcept{
        const auto& extent = rendering_resolution();
        return static_cast<float>(extent.width) / static_cast<float>(extent.height);
    }

    bool Renderer::ready_for_commands() const noexcept{
        if (!is_active()) {
            return false;
        }

        assert(frame_in_progress_ && "Frame not in progress");
        return frame_in_progress_;
    }
}
