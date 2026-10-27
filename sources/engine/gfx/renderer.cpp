#include "pch.h"
#include <nasral/gfx/renderer.h>
#include <nasral/gfx/renderer_defs.h>
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
        , is_frame_started_(false)
        , is_pass_started_(false)
        , is_surface_dirty_(false)
        , frame_count_(0)
        , frame_index_(0)
        , available_sc_image_(0)
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

        init_vk_swap_chain();
        log_info("Vulkan: Swap chain initialized.");

        init_vk_framebuffers();
        const auto extent_render = vk_offscreen_images_[FrameAttachmentType::eCompositing].front()->extent();
        const auto extent_swap_chain = vk_swap_chain_images_.front()->extent();
        const std::string extent_render_str = std::to_string(extent_render.width) + "x" + std::to_string(extent_render.height);
        const std::string extent_swap_chain_str = std::to_string(extent_swap_chain.width) + "x" + std::to_string(extent_swap_chain.height);
        log_info("Vulkan: Framebuffers initialized (" + extent_render_str + " -> " + extent_swap_chain_str + ").");

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
        // Если поверхность отображения изменилась
        if (is_surface_dirty_.exchange(false, std::memory_order_acquire)){
            refresh_vk_surface();
        }

        if (!is_active()) return;

        assert(!is_frame_started_ && "Frame already in progress");

        // Берем ссылки на ресурсы текущего кадра в полете
        const auto current_frame = frame_index();
        const auto& frame_fence = vk_frame_fence_[current_frame].get();
        const auto& render_semaphore = vk_render_available_semaphore_[current_frame].get();

        // 1. Сначала СТРОГО дожидаемся, пока GPU завершит работу с ресурсами ЭТОГО фрейма
        (void)vk_device_->logical_device().waitForFences(1u, &frame_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

        // 2. Сбрасываем кэш пайплайна
        vk_last_rasterization_pipeline_ = VK_NULL_HANDLE;

        // 3. Пытаемся получить картинку из swap chain ДО СБРОСА ФЕНСА
        try {
            const auto result = vk_device_->logical_device().acquireNextImageKHR(
                vk_swap_chain_.get(),
                std::numeric_limits<uint64_t>::max(),
                render_semaphore,
                VK_NULL_HANDLE,
                &available_sc_image_
            );

            if (result == vk::Result::eSuboptimalKHR) {
                mark_surface_dirty();
            }
        }
        catch (const ::vk::OutOfDateKHRError&) {
            mark_surface_dirty();
            return;
        }

        // 4. Точка невозврата: кадр успешно начат, сбрасываем фенс
        is_frame_started_ = true;
        (void)vk_device_->logical_device().resetFences(1u, &frame_fence);

        // 5. Открываем командный буфер
        vk_command_buffer().reset();
        vk_command_buffer().begin(vk::CommandBufferBeginInfo());
    }

    void Renderer::cmd_end_frame()
    {
        ENSURE_COMMANDS
        const auto current_frame = frame_index();

        // Закрываем командный буфер
        vk_command_buffer().end();

        // ИНДЕКСАЦИЯ СЕМАФОРОВ СТРОГО ПО current_frame (индексу кадра в полете)
        std::array<vk::Semaphore, 1> wait_semaphores{ vk_render_available_semaphore_[current_frame].get() };
        std::array<vk::Semaphore, 1> signal_semaphores{ vk_render_finished_semaphore_[current_frame].get() };
        std::array<vk::PipelineStageFlags, 1> wait_stages{ vk::PipelineStageFlagBits::eColorAttachmentOutput };

        // Получить очередь команд
        const auto& group = vk_device_->queue_group(static_cast<size_t>(CmdGroupType::eGraphicsAndPresent));
        auto& queue = group.queues.front();

        // Отправка команд текущего кадра на GPU
        try
        {
            queue.submit(vk::SubmitInfo()
                .setCommandBuffers(vk_command_buffer())
                .setWaitSemaphores(wait_semaphores)
                .setWaitDstStageMask(wait_stages)
                .setSignalSemaphores(signal_semaphores),
                vk_frame_fence_[current_frame].get());
        }
        catch (const ::vk::Error& e)
        {
            log_error("Vulkan: Failed to submit command buffer to queue (" + std::string(e.what()) + ")");
            is_frame_started_ = false; // Сбрасываем флаг, так как кадр сломался
            return;
        }

        // Показ готового изображения
        try {
            // Захватываем VkResult из презентации, так как Suboptimal не всегда генерирует Exception
            const vk::Result present_result = queue.presentKHR(vk::PresentInfoKHR()
                .setSwapchains(vk_swap_chain_.get())
                .setWaitSemaphores(signal_semaphores)
                .setImageIndices(available_sc_image_)); // Тут СТРОГО индекс картинки

            if (present_result == vk::Result::eSuboptimalKHR) {
                mark_surface_dirty();
            }
        }
        catch (const ::vk::OutOfDateKHRError&) {
            mark_surface_dirty();
        }
        catch (const ::vk::Error& e) {
            log_error("Vulkan: Present failed (" + std::string(e.what()) + ")");
        }

        // Кадр успешно завершен на стороне CPU (передан в очереди GPU)
        is_frame_started_ = false;

        // Обновляем индексы для СЛЕДУЮЩЕГО кадра
        ++frame_count_;
        frame_index_ = frame_count_ % static_cast<size_t>(config().max_frames_in_flight);
    }

    void Renderer::cmd_begin_rasterization_pass()
    {
        ENSURE_COMMANDS
        ENSURE_NO_PASS

        const auto extent = rendering_resolution();

        constexpr std::array color_types{
            FrameAttachmentType::eColor,
            FrameAttachmentType::eNormal,
            FrameAttachmentType::eEmission
        };

        // Размещаем массив барьеров на стеке без динамических аллокаций
        constexpr size_t kMaxBarriers = color_types.size() + 1;
        std::array<vk::ImageMemoryBarrier2, kMaxBarriers> image_barriers{};
        std::array<vk::RenderingAttachmentInfo, color_types.size()> color_attachments{};
        size_t barrier_count = 0;

        for (size_t i = 0; i < color_types.size(); ++i)
        {
            const auto type = color_types[i];
            const auto& image = vk_offscreen_images_[type][frame_index()];

            // Стадии и маски доступа теперь задаются индивидуально для каждого вложения
            image_barriers[barrier_count++] = vk::ImageMemoryBarrier2()
                .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
                .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(image->image())
                .setSubresourceRange(vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1));

            const auto attachment_clear = type == FrameAttachmentType::eColor
                ? as_vk_clear_color(config().clear_color)
                : vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);

            color_attachments[i] = vk::RenderingAttachmentInfo()
                .setImageView(image->render_attachment_view())
                .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setClearValue(vk::ClearValue(attachment_clear));
        }

        // Барьер для вложения глубины (со своими специфичными стадиями EarlyFragmentTests)
        const auto& depth_image = vk_offscreen_images_[FrameAttachmentType::eDepth][frame_index_];
        image_barriers[barrier_count++] = vk::ImageMemoryBarrier2()
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
            .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(depth_image->image())
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1));

        // Единый вызов pipelineBarrier2 через DependencyInfo
        vk_command_buffer().pipelineBarrier2(
            vk::DependencyInfo()
                .setPImageMemoryBarriers(image_barriers.data())
                .setImageMemoryBarrierCount(barrier_count),
            vk_loader_
        );

        const auto depth_attachment = vk::RenderingAttachmentInfo()
            .setImageView(depth_image->render_attachment_view())
            .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)));

        vk::RenderingInfo rendering_info{};
        rendering_info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), extent));
        rendering_info.setLayerCount(1);
        rendering_info.setColorAttachments(color_attachments);
        rendering_info.setPDepthAttachment(&depth_attachment);
        vk_command_buffer().beginRendering(rendering_info, vk_loader_);

        is_pass_started_ = true;

        const auto& pipeline_layout = vk_uniform_layout(ShaderProgramType::eRasterization).vk_pipeline_layout();
        const std::array descriptor_sets{
            vk_rendering_d_sets_[UniformRasterDSetType::eViewUBO].get(),
            vk_rendering_d_sets_[UniformRasterDSetType::eObjectUBOs].get(),
            vk_rendering_d_sets_[UniformRasterDSetType::eMaterialUBOs].get(),
            vk_rendering_d_sets_[UniformRasterDSetType::eMaterialTextures].get(),
            vk_rendering_d_sets_[UniformRasterDSetType::eLightUBOs].get()
        };
        vk_command_buffer().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptor_sets, {});
    }

    void Renderer::cmd_end_rasterization_pass()
    {
        ENSURE_COMMANDS
        ENSURE_PASS

        vk_command_buffer().endRendering();

        constexpr std::array color_types{
            FrameAttachmentType::eColor,
            FrameAttachmentType::eNormal,
            FrameAttachmentType::eEmission
        };

        constexpr size_t kMaxBarriers = color_types.size() + 1;
        std::array<vk::ImageMemoryBarrier2, kMaxBarriers> image_barriers{};
        size_t barrier_count = 0;

        // 1. Цветовые вложения: ColorAttachmentWrite -> ShaderRead (FragmentShader)
        for (const auto type : color_types)
        {
            const auto& image = vk_offscreen_images_[type][frame_index()];
            image_barriers[barrier_count++] = vk::ImageMemoryBarrier2()
                .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
                .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
                .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(image->image())
                .setSubresourceRange(vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1));
        }

        // 2. Вложение глубины: DepthStencilAttachmentWrite -> ShaderRead (FragmentShader)
        const auto& depth_image = vk_offscreen_images_[FrameAttachmentType::eDepth][frame_index_];
        image_barriers[barrier_count++] = vk::ImageMemoryBarrier2()
            .setSrcStageMask(vk::PipelineStageFlagBits2::eLateFragmentTests)
            .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
            .setOldLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(depth_image->image())
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1));

        // Батчинг цвета и глубины в один синхронизационный вызов
        vk_command_buffer().pipelineBarrier2(
            vk::DependencyInfo()
                .setPImageMemoryBarriers(image_barriers.data())
                .setImageMemoryBarrierCount(barrier_count),
            vk_loader_
        );

        is_pass_started_ = false;
        vk_last_rasterization_pipeline_ = VK_NULL_HANDLE;
    }

    void Renderer::cmd_begin_screen_pass(const EnumMask<FrameAttachmentType> input, const EnumMask<FrameAttachmentType> output)
    {
        ENSURE_COMMANDS
        ENSURE_NO_PASS

        // 1. Валидация входных аргументов ДО изменения состояния движка
        if (!(input.any() && output.any())) {
            log_error("Screen pass input and output attachments must be specified.");
            return;
        }

        // На текущий момент поддерживается только один цветовой выходной буфер.
        if (output.bitset.count() > 1) {
            log_error("Screen pass currently supports a single color output attachment.");
            return;
        }

        // Не поддерживается глубина в качестве выходного буфера.
        if (output.test(FrameAttachmentType::eDepth)) {
            log_error("Screen pass output cannot be a depth attachment.");
            return;
        }

        // Выходные и входные вложения не могут совпадать.
        if constexpr (kDebugBuild) {
            for (const auto type : magic_enum::enum_values<FrameAttachmentType>()) {
                if (type != FrameAttachmentType::TOTAL && input.test(type) && output.test(type)) {
                    log_error("Screen pass cannot sample from the same attachment it renders to.");
                    return;
                }
            }
        }

        // Проверки пройдены успешно — начинаем проход
        is_pass_started_ = true;

        const auto extent = rendering_resolution();
        last_screen_pass_output_attachments_ = output;

        // Массивы на стеке без динамических аллокаций
        constexpr auto kMaxAttachments = static_cast<size_t>(FrameAttachmentType::TOTAL);
        std::array<vk::ImageMemoryBarrier2, kMaxAttachments> image_barriers{};
        std::array<vk::RenderingAttachmentInfo, kMaxAttachments> color_attachments{};
        size_t attachment_count = 0;

        // 2. Подготовка выходных цветовых вложений
        for (const auto type : magic_enum::enum_values<FrameAttachmentType>())
        {
            if (type == FrameAttachmentType::TOTAL || !output.test(type)) {
                continue;
            }

            const auto& image = vk_offscreen_images_[type][frame_index()];

            // Защита от WAR-hazard (Write-After-Read):
            // Гарантируем, что предыдущие проходы закончили читать это вложение в FragmentShader,
            // прежде чем текущий проход начнет переводить его и писать в него цвет.
            image_barriers[attachment_count] = vk::ImageMemoryBarrier2()
                .setSrcStageMask(vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eColorAttachmentWrite)
                .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(image->image())
                .setSubresourceRange(vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1));

            // Описание цветового вложения с очисткой
            color_attachments[attachment_count] = vk::RenderingAttachmentInfo()
                .setImageView(image->render_attachment_view())
                .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setClearValue(vk::ClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f)));

            ++attachment_count;
        }

        // 3. Вызов единого барьера Synchronization2
        vk_command_buffer().pipelineBarrier2(
            vk::DependencyInfo()
                .setPImageMemoryBarriers(image_barriers.data())
                .setImageMemoryBarrierCount(attachment_count),
            vk_loader_
        );

        // 4. Начать рендеринг во вложения
        vk::RenderingInfo rendering_info{};
        rendering_info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), extent));
        rendering_info.setLayerCount(1);
        rendering_info.setPColorAttachments(color_attachments.data());
        rendering_info.setColorAttachmentCount(static_cast<uint32_t>(attachment_count));
        vk_command_buffer().beginRendering(rendering_info, vk_loader_);

        // 5. Привязать наборы дескрипторов экранного прохода
        const auto& pipeline_layout = vk_uniform_layout(ShaderProgramType::eScreenPass).vk_pipeline_layout();
        const std::array descriptor_sets{
            vk_screen_d_sets_per_frame_[frame_index()].get(),
            vk_screen_d_sets_[UniformScreenDSetType::eViewUBO].get(),
            vk_screen_d_sets_[UniformScreenDSetType::eScreenPassUBOs].get(),
        };
        vk_command_buffer().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptor_sets, {});
    }

    void Renderer::cmd_end_screen_pass()
    {
        ENSURE_COMMANDS
        ENSURE_PASS

        vk_command_buffer().endRendering();

        // Массив барьеров на стеке для всех переводимых вложений
        constexpr auto kMaxAttachments = static_cast<size_t>(FrameAttachmentType::TOTAL);
        std::array<vk::ImageMemoryBarrier2, kMaxAttachments> image_barriers{};
        size_t barrier_count = 0;

        // Перевод всех задействованных в проходе выходных вложений:
        // ColorAttachmentWrite (вывод цвета) -> ShaderRead (чтение в фрагментном шейдере следующих проходов)
        for (const auto type : magic_enum::enum_values<FrameAttachmentType>())
        {
            if (type == FrameAttachmentType::TOTAL || !last_screen_pass_output_attachments_.test(type)) {
                continue;
            }

            const auto& image = vk_offscreen_images_[type][frame_index()];

            image_barriers[barrier_count++] = vk::ImageMemoryBarrier2()
                .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
                .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
                .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(image->image())
                .setSubresourceRange(vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1));
        }

        if (barrier_count > 0)
        {
            vk_command_buffer().pipelineBarrier2(
                vk::DependencyInfo()
                    .setPImageMemoryBarriers(image_barriers.data())
                    .setImageMemoryBarrierCount(barrier_count),
                vk_loader_
            );
        }

        // Проход завершен
        is_pass_started_ = false;
        last_screen_pass_output_attachments_.clear();
    }

    void Renderer::cmd_bind_shader_program(const handles::ShaderProgram &handles)
    {
        ENSURE_COMMANDS
        ENSURE_PASS

        // Текущее разрешение рендеринга
        const auto extent = rendering_resolution();

        // Область вида (динамическое состояние конвейера)
        auto viewport = vk::Viewport()
            .setX(0.0f)
            .setWidth(static_cast<float>(extent.width))
            .setMinDepth(0.0f)
            .setMaxDepth(1.0f);

        // Совместимость координат с OpenGL
        if (config().opengl_compatible){
            viewport.setY(static_cast<float>(extent.height));
            viewport.setHeight(-static_cast<float>(extent.height));
        }else{
            viewport.setY(0.0f);
            viewport.setHeight(static_cast<float>(extent.height));
        }

        // Ножницы (динамическое состояние конвейера)
        auto scissor = vk::Rect2D()
            .setOffset(vk::Offset2D(0, 0))
            .setExtent(extent);

        // Если конвейер сменился - привязать
        if (vk_last_rasterization_pipeline_ != handles.pipeline){
            vk_command_buffer().bindPipeline(vk::PipelineBindPoint::eGraphics, handles.pipeline);
        }

        // Обновить последний привязанный конвейер
        vk_last_rasterization_pipeline_ = handles.pipeline;

        // Запись команд. Привязать динамические состояния
        vk_command_buffer().setViewport(0, {viewport});
        vk_command_buffer().setScissor(0, {scissor});
    }

    void Renderer::cmd_bind_mesh_geometry(const handles::Mesh &handles) const
    {
        ENSURE_COMMANDS
        ENSURE_PASS

        // Запись команд. Привязать геометрию
        vk_command_buffer().bindVertexBuffers(0, {handles.vertex_buffer}, {0});
        vk_command_buffer().bindIndexBuffer(handles.index_buffer, 0, vk::IndexType::eUint32);
    }

    void Renderer::cmd_draw_bound_geometry(
        const uint32_t index_offset,
        const uint32_t index_count,
        const uint32_t obj_uniform_idx,
        const uint32_t mat_uniform_idx)
    {
        ENSURE_COMMANDS
        ENSURE_PASS

        // Командный буфер кадра и макет конвейера
        auto& pipeline_layout = vk_uniform_layouts_[ShaderProgramType::eRasterization]->vk_pipeline_layout();

        // Push-константы - uniform индекс материала и объекта (порядок важен!)
        const std::array<uint32_t, 2> push_constants{
            mat_uniform_idx,
            obj_uniform_idx,
        };

        // Передать push-константы
        vk_command_buffer().pushConstants(
            pipeline_layout,
            vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(uint32_t) * 2,
            push_constants.data());

        // Рисование геометрии
        vk_command_buffer().drawIndexed(index_count, 1, index_offset, 0, 0);
    }

    void Renderer::cmd_draw_screen_quad(const uint32_t pass_uniform_idx)
    {
        ENSURE_COMMANDS
        ENSURE_PASS

        // Макет конвейера
        auto& pipeline_layout = vk_uniform_layouts_[ShaderProgramType::eScreenPass]->vk_pipeline_layout();

        // Передать push-константы
        vk_command_buffer().pushConstants(
            pipeline_layout,
            vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(uint32_t),
            &pass_uniform_idx);

        // Нарисовать 2 треугольника
        vk_command_buffer().draw(6, 1, 0, 0);
    }

    void Renderer::cmd_gen_frame_mipmaps(const FrameAttachmentType &type) const
    {
        ENSURE_COMMANDS

        // Запись команд генерации мип-уровней
        vk_offscreen_image(type).write_gen_mipmaps_commands(
            vk_command_buffer(),
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::ImageAspectFlagBits::eColor);
    }

    void Renderer::cmd_blit_to_screen(const FrameAttachmentType &type) const
    {
        ENSURE_COMMANDS
        ENSURE_NO_PASS

        const auto& src_image = vk_offscreen_image(type);
        const auto& dst_image = vk_swap_chain_image();

        const auto& src_extent = src_image.extent();
        const auto& dst_extent = dst_image.extent();

        // 1. Барьеры перед blit:
        // - Исходное вложение: ShaderReadOnlyOptimal -> TransferSrcOptimal
        // - SwapChain картинка: Undefined -> TransferDstOptimal
        std::array<vk::ImageMemoryBarrier2, 2> pre_barriers{};

        pre_barriers[0] = vk::ImageMemoryBarrier2()
            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput | vk::PipelineStageFlagBits2::eFragmentShader)
            .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eShaderRead)
            .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setDstAccessMask(vk::AccessFlagBits2::eTransferRead)
            .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(src_image.image())
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1));

        pre_barriers[1] = vk::ImageMemoryBarrier2()
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(dst_image.image())
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1));

        vk_command_buffer().pipelineBarrier2(
            vk::DependencyInfo()
                .setPImageMemoryBarriers(pre_barriers.data())
                .setImageMemoryBarrierCount(static_cast<uint32_t>(pre_barriers.size())),
            vk_loader_
        );

        // 2. Выполнение blit (с масштабированием и линейной фильтрацией, если разрешения отличаются)
        const vk::ImageBlit blit_region = vk::ImageBlit()
            .setSrcSubresource(vk::ImageSubresourceLayers()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setMipLevel(0)
                .setBaseArrayLayer(0)
                .setLayerCount(1))
            .setSrcOffsets({
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{static_cast<int32_t>(src_extent.width), static_cast<int32_t>(src_extent.height), 1}
            })
            .setDstSubresource(vk::ImageSubresourceLayers()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setMipLevel(0)
                .setBaseArrayLayer(0)
                .setLayerCount(1))
            .setDstOffsets({
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{static_cast<int32_t>(dst_extent.width), static_cast<int32_t>(dst_extent.height), 1}
            });

        vk_command_buffer().blitImage(
            src_image.image(), vk::ImageLayout::eTransferSrcOptimal,
            dst_image.image(), vk::ImageLayout::eTransferDstOptimal,
            {blit_region},
            vk::Filter::eLinear
        );

        // 3. Барьеры после blit:
        // - Исходное вложение: TransferSrcOptimal -> ShaderReadOnlyOptimal (возвращаем в исходное)
        // - SwapChain картинка: TransferDstOptimal -> PresentSrcKHR (готова к отправке на монитор)
        std::array<vk::ImageMemoryBarrier2, 2> post_barriers{};

        post_barriers[0] = vk::ImageMemoryBarrier2()
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferRead)
            .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
            .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(src_image.image())
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1));

        post_barriers[1] = vk::ImageMemoryBarrier2()
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
            .setDstAccessMask(vk::AccessFlagBits2::eNone)
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(dst_image.image())
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1));

        vk_command_buffer().pipelineBarrier2(
            vk::DependencyInfo()
                .setPImageMemoryBarriers(post_barriers.data())
                .setImageMemoryBarrierCount(static_cast<uint32_t>(post_barriers.size())),
            vk_loader_
        );
    }

    void Renderer::cmd_wait_for_all() const
    {
        vk_device_->logical_device().waitIdle();
    }

#pragma endregion

    void Renderer::mark_surface_dirty()
    {
        is_surface_dirty_.store(true, std::memory_order_release);
    }

    const vk::Extent2D & Renderer::rendering_resolution() const noexcept
    {
        return config().rendering_resolution;
    }

    const vk::Extent2D & Renderer::output_resolution() const noexcept
    {
        return vk_device_->clamp_swapchain_extent(
            config().surface_provider->framebuffer_extent(),
            *vk_surface_);
    }

    float Renderer::aspect_ratio() const noexcept
    {
        const auto& extent = rendering_resolution();
        return static_cast<float>(extent.width) / static_cast<float>(extent.height);
    }

    vk::ImageAspectFlags Renderer::frame_attachment_aspect(
        const FrameAttachmentType &type,
        const vk::Format &format)
    {
        // Если это не глубина, то используется как цветовое вложение
        if (type != FrameAttachmentType::eDepth) {
            return vk::ImageAspectFlagBits::eColor;
        }

        // В остальных случаях используется как глубина
        auto aspect = vk::ImageAspectFlags(vk::ImageAspectFlagBits::eDepth);

        // Если формат содержит S8Uint, то используется как глубина и stencil
        if (format == vk::Format::eD16UnormS8Uint
            || format == vk::Format::eD24UnormS8Uint
            || format == vk::Format::eD32SfloatS8Uint)
        {
            aspect |= vk::ImageAspectFlagBits::eStencil;
        }
        return aspect;
    }
}
