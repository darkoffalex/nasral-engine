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
        , vk_last_pipeline_(VK_NULL_HANDLE)
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
        const auto extent = vk_framebuffers_[0]->extent();
        const std::string extent_str = std::to_string(extent.width) + "x" + std::to_string(extent.height);
        log_info("Vulkan: Framebuffers initialized (" + extent_str + ").");

        init_vk_uniform_layouts();
        log_info("Vulkan: Uniform layouts initialized.");

        init_vk_texture_samplers();
        log_info("Vulkan: Texture samplers initialized.");

        init_vk_uniforms();
        log_info("Vulkan: Uniforms initialized.");

        init_vk_command_buffers();
        log_info("Vulkan: Command buffers initialized.");

        init_vk_synchronization();
        log_info("Vulkan: Synchronization initialized.");

        is_active_ = true;

        log_info("Renderer initialized.");
    }

    Renderer::~Renderer()
    {
        try
        {
            // Остановить рендеринг
            is_active_ = false;

            // Дождаться завершения кадра
            cmd_wait_for_frame();

            log_info("Renderer finalized");
        }
        catch (...)
        {
        }
    }

#pragma region render_commands

    void Renderer::cmd_begin_frame()
    {
        // Если требуется обновить поверхность отображения (размеры и прочее)
        if (surface_refresh_needed_.exchange(false, std::memory_order_acquire)){
            refresh_vk_surface();
        }

        // Если рендеринг деактивирован - выйти
        if (!is_active()) return;

        // Кадр начат
        assert(frame_in_progress_ == false && "Frame already in progress");
        frame_in_progress_ = true;

        // Сброс последнего использованного конвейера перед началом кадра (группировка и кеш материалов)
        vk_last_pipeline_ = VK_NULL_HANDLE;

        // Описываем очистку вложений кадрового буфера (цвет, глубина/трафарет)
        std::array<vk::ClearValue, 2> clear_values{};

        clear_values[0].color = vk::ClearColorValue(
            config().clear_color.r,
            config().clear_color.g,
            config().clear_color.b,
            config().clear_color.a);

        clear_values[1].depthStencil = vk::ClearDepthStencilValue(
            1.0f,
            0);

        // Ожидаем завершения кадра с текущим индексом (на случай если он еще не готов)
        // Функция блокирует поток при ожидании барьера
        (void)vk_device_->logical_device().waitForFences(
            1u,
            &vk_frame_fence_[frame()].get(),
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        // Сброс барьера кадра
        (void)vk_device_->logical_device().resetFences(
            1u,
            &vk_frame_fence_[frame()].get());

        // Получить доступное изображение swap-chain
        // Функция блокирует поток до получения доступного изображения.
        const auto result = vk_device_->logical_device().acquireNextImageKHR(
            vk_swap_chain_.get(),
            std::numeric_limits<uint64_t>::max(),
            vk_render_available_semaphore_[frame()].get(),
            VK_NULL_HANDLE,
            &available_image_index_);

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame()];

        // Если изображение было получено
        if (result == vk::Result::eSuccess)
        {
            // Размеры области рендеринга
            const auto& extent = vk_framebuffers_[available_image_index_]->extent();
            const auto& width = extent.width;
            const auto& height = extent.height;

            // Получить буфер кадра
            auto& frame_buffer = vk_framebuffers_[available_image_index_]->vk_framebuffer();

            // Начать работу с буфером команд
            cmd_buffer->reset();
            cmd_buffer->begin(vk::CommandBufferBeginInfo());

            // Начать проход рендеринга, используя полученный ранее кадровый буфер
            cmd_buffer->beginRenderPass(
                vk::RenderPassBeginInfo()
                .setRenderPass(vk_render_pass_.get())
                .setFramebuffer(frame_buffer)
                .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)))
                .setClearValues(clear_values),
                vk::SubpassContents::eInline);
        }
        // Если не удалось, возможно, изменилась поверхность - обновить
        else if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
        {
            request_surface_refresh();
        }

        /* Дескрипторные наборы */

        // Получить макет конвейера
        const auto& pipeline_l = vk_uniform_layouts_[UniformLayoutType::eRasterization]->vk_pipeline_layout();

        // Привязать все необходимые дескрипторы
        cmd_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_l, 0,
            {
                vk_dset_view_.get(),
                vk_dset_objects_uniforms_.get(),
                vk_dset_material_uniforms_.get(),
                vk_dset_material_textures_.get(),
                vk_dset_light_sources_.get()
            },
            {});
    }

    void Renderer::cmd_end_frame()
    {
        if (!ready_for_commands()){
            return;
        }

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame()];

        // Завершение прохода (неявное преобразование кадра в VK_IMAGE_LAYOUT_PRESENT_SRC_KHR для представления)
        cmd_buffer->endRenderPass();

        // Завершения командного буфера
        cmd_buffer->end();

        // Семафоры, ожидаемые для исполнения команд рендеринга
        std::array<vk::Semaphore, 1> wait_semaphores{
            vk_render_available_semaphore_[frame()].get()
        };

        // Семафоры, сигнализирующие готовность к показу
        std::array<vk::Semaphore, 1> signal_semaphores{
            vk_render_finished_semaphore_[available_image_index_].get()
        };

        // Стадии, на которых конвейер будет ждать wait_semaphores
        std::array<vk::PipelineStageFlags, 1> wait_stages{
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        // Отправить командные буферы на исполнение
        const auto& group = vk_device_->queue_group(static_cast<size_t>(CmdGroupType::eGraphicsAndPresent));
        auto& queue = group.queues[0];

        // Подача команд рендеринга в очередь
        queue.submit(vk::SubmitInfo()
            .setCommandBuffers(cmd_buffer.get())
            .setWaitSemaphores(wait_semaphores)
            .setWaitDstStageMask(wait_stages)
            .setSignalSemaphores(signal_semaphores),
            vk_frame_fence_[frame()].get());

        try
        {
            // Подача команд показа в очередь
            const auto result = queue.presentKHR(vk::PresentInfoKHR()
                .setSwapchains(vk_swap_chain_.get())
                .setWaitSemaphores(signal_semaphores)
                .setImageIndices(available_image_index_));

            if (result == vk::Result::eErrorOutOfDateKHR){
                request_surface_refresh();
                return;
            }
        }
        catch(const ::vk::OutOfDateKHRError&){
            frame_in_progress_ = false;
            request_surface_refresh();
            return;
        }

        // Обновить счетчики кадров
        ++frame_count_;
        frame_index_ = frame_count_ % static_cast<size_t>(config().max_frames_in_flight);

        // Кадр завершен
        frame_in_progress_ = false;
    }

    void Renderer::cmd_bind_material(const handles::Material& handles, const uint32_t uniform_idx)
    {
        if (!ready_for_commands()){
            return;
        }

        // Размеры области рендеринга
        const auto& extent = vk_framebuffers_[available_image_index_]->extent();
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
        if (vk_last_pipeline_ != handles.pipeline){
            cmd_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, handles.pipeline);
        }

        // Обновить последний привязанный конвейер
        vk_last_pipeline_ = handles.pipeline;

        // Запись команд. Привязать динамические состояния
        cmd_buffer->setViewport(0, {viewport});
        cmd_buffer->setScissor(0, {scissor});
    }

    void Renderer::cmd_bind_geometry(const handles::Mesh& handles, const uint32_t uniform_idx)
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

    void Renderer::cmd_wait_for_frame() const
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
        cmd_wait_for_frame();

        // Отключить рендеринг и сбросить кадр
        is_active_ = false;
        frame_count_ = 0;
        frame_index_ = 0;

        // Очистить командные буферы
        vk_command_buffers_.clear();
        log_info("Vulkan: command buffers cleared.");

        // Уничтожить кадровые буферы
        vk_framebuffers_.clear();
        log_info("Vulkan: framebuffers cleared.");

        // Пере-создать swap-chain (старый будет задействован при создании нового, затем удален)
        init_vk_swap_chain();
        log_info("Vulkan: swap-chain recreated.");

        // Создать новые кадровые буферы
        init_vk_framebuffers();
        const auto extent = vk_framebuffers_[0]->extent();
        const std::string extent_str = std::to_string(extent.width) + "x" + std::to_string(extent.height);
        log_info("Vulkan: framebuffers created. Swapchain extent: " + extent_str);

        // Создать новые командные буферы
        init_vk_command_buffers();
        log_info("Vulkan: command buffers recreated.");

        // Включить рендеринг
        is_active_ = true;
    }

    const vk::Extent2D& Renderer::rendering_resolution() const noexcept{
        return vk_framebuffers_[0]->extent();
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
