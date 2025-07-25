#include "pch.h"
#include <nasral/rendering/renderer.h>
#include <nasral/engine.h>

namespace nasral::rendering
{
    Renderer::Renderer(const Engine *engine, RenderingConfig config)
        : engine_(engine)
        , config_(std::move(config))
        , is_rendering_(false)
        , surface_refresh_required_(false)
        , current_frame_(0)
        , available_image_index_(0)
    {
        logger()->info("Initializing renderer...");

        try {
            init_vk_instance();
            logger()->info("Vulkan: Instance created.");

            init_vk_loader();
            logger()->info("Vulkan: Loader created.");

            init_vk_debug_callback();
            logger()->info("Vulkan: Debug callback created.");

            init_vk_surface();
            logger()->info("Vulkan: Surface created.");

            init_vk_device();
            const auto device_name = vk_device_->physical_device().getProperties().deviceName;
            const auto device_name_str = std::string(device_name.data(), strlen(device_name.data()));
            logger()->info("Vulkan: Device initialized (" + device_name_str + ").");

            init_vk_render_passes();
            logger()->info("Vulkan: Render passes created.");

            init_vk_swap_chain();
            logger()->info("Vulkan: Swap chain created.");

            init_vk_framebuffers();
            const auto extent = vk_framebuffers_[0]->extent();
            const std::string extent_str = std::to_string(extent.width) + "x" + std::to_string(extent.height);
            logger()->info("Vulkan: Frame buffers created (" + extent_str + ").");

            init_vk_descriptor_pool();
            logger()->info("Vulkan: Descriptor pool created.");

            init_vk_descriptor_set_layouts();
            logger()->info("Vulkan: Descriptor set layouts created.");

            init_vk_uniforms();
            logger()->info("Vulkan: Uniform buffers allocated.");

            init_vk_command_buffers();
            logger()->info("Vulkan: Command buffers created.");

            init_vk_sync_objects();
            logger()->info("Vulkan: Sync primitives created.");

            is_rendering_ = true;
        }
        catch (const std::exception& e) {
            throw RenderingError(e.what());
        }
    }

    Renderer::~Renderer()
    {
        is_rendering_ = false;
        cmd_wait_for_frame();
    }

    void Renderer::cmd_begin_frame(){
        // Если требуется обновить поверхность и связанные с ней кадровые буферы
        if (surface_refresh_required_.exchange(false, std::memory_order_acquire)){
            refresh_vk_surface();
        }

        // Если рендеринг отключен
        if (!is_rendering_) return;

        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);

        // Размеры области рендеринга
        const auto& extent = vk_framebuffers_[frame_index]->extent();
        const auto& width = extent.width;
        const auto& height = extent.height;

        // Описываем очистку вложений кадрового буфера (цвет, глубина/трафарет)
        std::array<vk::ClearValue, 2> clear_values{};
        clear_values[0].color = vk::ClearColorValue(config_.clear_color);
        clear_values[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

        // Ожидаем завершения кадра с текущим индексом (на случай если он еще не готов)
        // Функция блокирует поток при ожидании барьера
        (void)vk_device_->logical_device().waitForFences(
            1u,
            &vk_frame_fence_[frame_index].get(),
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        // Сброс барьера кадра
        (void)vk_device_->logical_device().resetFences(
            1u,
            &vk_frame_fence_[frame_index].get());

        // Получить доступное изображение swap-chain
        // Функция блокирует поток до получения доступного изображения.
        const auto result = vk_device_->logical_device().acquireNextImageKHR(
            vk_swap_chain_.get(),
            std::numeric_limits<uint64_t>::max(),
            vk_render_available_semaphore_[frame_index].get(),
            VK_NULL_HANDLE,
            &available_image_index_);

        // Если изображение было получено
        if (result == vk::Result::eSuccess){
            // Получить буфер кадра и команд
            auto& cmd_buffer = vk_command_buffers_[frame_index];
            auto& frame_buffer = vk_framebuffers_[available_image_index_]->vk_framebuffer();

            // Начать работу с буфером команд
            cmd_buffer->reset();
            cmd_buffer->begin(vk::CommandBufferBeginInfo());

            // Начать проход рендеринга, используя полученный ранее кадровый буфер
            cmd_buffer->beginRenderPass(
                vk::RenderPassBeginInfo()
                .setRenderPass(vk_render_pass_.get())
                .setFramebuffer(frame_buffer)
                .setRenderArea(vk::Rect2D(
                    vk::Offset2D(0, 0),
                    vk::Extent2D(width, height)))
                .setClearValues(clear_values),
                vk::SubpassContents::eInline);
        }
        // Возможно требуется пересоздание swap-chain
        else if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR){
            request_surface_refresh();
        }
    }

    void Renderer::cmd_end_frame(){
        // Если рендеринг отключен
        if (!is_rendering_) return;

        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame_index];

        // Завершение прохода (неявное преобразование кадра в VK_IMAGE_LAYOUT_PRESENT_SRC_KHR для представления)
        cmd_buffer->endRenderPass();

        // Завершения командного буфера
        cmd_buffer->end();

        // Семафоры, ожидаемые для исполнения команд рендеринга
        std::array<vk::Semaphore, 1> wait_semaphores{
            vk_render_available_semaphore_[frame_index].get()
        };

        // Семафоры, сигнализирующие готовность к показу
        std::array<vk::Semaphore, 1> signal_semaphores{
            vk_render_finished_semaphore_[frame_index].get()
        };

        // Стадии, на которых конвейер будет ждать wait_semaphores
        std::array<vk::PipelineStageFlags, 1> wait_stages{
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        // Отправить командные буферы на исполнение
        const auto& group = vk_device_->queue_group(to<size_t>(CommandGroup::eGraphicsAndPresent));
        auto& queue = group.queues[0];

        // Подача команд рендеринга в очередь
        queue.submit(vk::SubmitInfo()
            .setCommandBuffers(cmd_buffer.get())
            .setWaitSemaphores(wait_semaphores)
            .setWaitDstStageMask(wait_stages)
            .setSignalSemaphores(signal_semaphores),
            vk_frame_fence_[frame_index].get());

        // В случае ошибки показа - вероятно требуется пересоздание swap-chain
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
            request_surface_refresh();
            return;
        }

        // Инкремент счетчика кадров
        current_frame_++;
    }

    void Renderer::cmd_bind_material_pipeline(const Handles::Material& handles){
        // Если рендеринг отключен
        if (!is_rendering_) return;

        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame_index];

        // Размеры области рендеринга
        const auto& extent = vk_framebuffers_[frame_index]->extent();
        const auto& width = extent.width;
        const auto& height = extent.height;

        // Область вида (динамическое состояние)
        auto viewport = vk::Viewport()
        .setX(0.0f)
        .setWidth(static_cast<float>(width))
        .setMinDepth(0.0f)
        .setMaxDepth(1.0f);

        if (config_.use_opengl_style){
            viewport.setY(static_cast<float>(height));
            viewport.setHeight(-static_cast<float>(height));
        }else{
            viewport.setY(0.0f);
            viewport.setHeight(static_cast<float>(height));
        }

        // Ножницы (динамическое состояние)
        auto scissor = vk::Rect2D()
        .setOffset(vk::Offset2D(0, 0))
        .setExtent(extent);

        // Запись команд. Привязать конвейер и динамические состояния
        cmd_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, handles.pipeline);
        cmd_buffer->setViewport(0, {viewport});
        cmd_buffer->setScissor(0, {scissor});
    }

    void Renderer::cmd_draw_mesh(const Handles::Mesh& handles){
        // Если рендеринг отключен
        if (!is_rendering_) return;

        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame_index];

        // Запись команд. Привязать геометрию и нарисовать её
        cmd_buffer->bindVertexBuffers(0, {handles.vertex_buffer}, {0});
        cmd_buffer->bindIndexBuffer(handles.index_buffer, 0, vk::IndexType::eUint32);
        cmd_buffer->drawIndexed(handles.index_count, 1, 0, 0, 0);
    }

    void Renderer::cmd_wait_for_frame() const{
        vk_device_->logical_device().waitIdle();
    }

    void Renderer::request_surface_refresh(){
        surface_refresh_required_.store(true, std::memory_order_release);
    }

    vk::Extent2D Renderer::get_rendering_resolution() const{
        assert(!vk_framebuffers_.empty());
        return vk_framebuffers_[0]->extent();
    }

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
        const auto* logger = static_cast<logging::Logger*>(user_data);
        const std::string msg_str = "Vulkan validation: " + std::string(layer_prefix) + " | " + std::string(msg) + "\n";
        logger->warning(msg_str);
        return VK_FALSE;
    }

    const logging::Logger* Renderer::logger() const{
        return engine()->logger();
    }

    void Renderer::init_vk_instance()
    {
        // Требуемые расширения и слои
        std::vector<const char*> req_extensions = config_.surface_provider->surface_extensions();
        std::vector<const char*> req_layers = {};

        // Если нужна валидация
        if (config_.use_validation_layers){
            req_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            req_layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        // Информация о приложении
        const auto app_info = vk::ApplicationInfo()
            .setPApplicationName(config_.app_name.c_str())
            .setPEngineName(config_.engine_name.c_str())
            .setApiVersion(VK_API_VERSION_1_4)
            .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
            .setEngineVersion(VK_MAKE_VERSION(1, 0, 0));

        // Создать сущность Vulkan
        vk_instance_ = vk::createInstanceUnique(vk::InstanceCreateInfo()
            .setPEnabledExtensionNames(req_extensions)
            .setPEnabledLayerNames(req_layers)
            .setPApplicationInfo(&app_info));
    }

    void Renderer::init_vk_loader(){
        assert(vk_instance_);
        vk_dispatch_loader_ = vk::detail::DispatchLoaderDynamic(vk_instance_.get(), config_.pfn_vk_get_proc_addr);
        vk_dispatch_loader_.init(vk_instance_.get());
    }

    void Renderer::init_vk_debug_callback(){
        if (!config_.use_validation_layers) return;
        assert(vk_instance_);

        // Используем статический метод vk_debug_report_callback для логирования предупреждений и ошибок от слоев.
        // Передаем logger как пользовательский указатель
        vk_debug_callback_ = vk_instance_->createDebugReportCallbackEXTUnique(
            vk::DebugReportCallbackCreateInfoEXT()
                .setFlags(vk::DebugReportFlagBitsEXT::eError|vk::DebugReportFlagBitsEXT::eWarning)
                .setPfnCallback(reinterpret_cast<vk::PFN_DebugReportCallbackEXT>(vk_debug_report_callback))
                .setPUserData(const_cast<logging::Logger*>(logger())),
            nullptr,
            vk_dispatch_loader_);
    }

    void Renderer::init_vk_surface(){
        assert(vk_instance_);
        // Поверхность создается вне системы рендеринга (на стороне приложения, например, при помощи GLFW)
        const vk::SurfaceKHR surface = config_.surface_provider->create_surface(*vk_instance_);

        // Поскольку был создан "голый" handler, нужно обернуть его в unique pointer
        // Также нужно предоставить функтор удаления (второй аргумент, используем стандартный)
        vk_surface_ = vk::UniqueSurfaceKHR(
            surface,
            ::vk::detail::ObjectDestroy<::vk::Instance, ::vk::detail::DispatchLoaderStatic>(vk_instance_.get()));
    }

    void Renderer::init_vk_device(){
        assert(vk_instance_);
        assert(vk_surface_);

        // Требуемые расширения (поддержка своп-чейна и выделенных аллокаций памяти)
        const std::vector req_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME
        };

        // Требования к очередям
        std::vector<vk::utils::Device::QueueGroupRequest> req_queues(to<size_t>(CommandGroup::TOTAL));
        req_queues[to<size_t>(CommandGroup::eGraphicsAndPresent)] = vk::utils::Device::QueueGroupRequest::graphics(1, true),
        req_queues[to<size_t>(CommandGroup::eTransfer)] = vk::utils::Device::QueueGroupRequest::transfer(1),

        // Создать устройство
        vk_device_ = std::make_unique<vk::utils::Device>(
            vk_instance_,
            vk_surface_,
            req_queues,
            req_extensions);
    }

    void Renderer::init_vk_render_passes(){
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        if (!vk_device_->supports_color(config_.color_format, vk_surface_)){
            throw std::runtime_error("Color format is not supported by the device");
        }

        if (!vk_device_->supports_depth(config_.depth_stencil_format)){
            throw std::runtime_error("Depth stencil format is not supported by the device");
        }

        /**
         * Общая теория:
         *
         * В отличии от OpenGL, где проход редеринга абстракция чисто програмная, в Vulkan проход это отдельный объект.
         * Он описывает как ведет себя память вложений (цвет, глубина) на разных этапах конвейера рендеринга. У каждого
         * прохода есть свои под-проходы, они позволяют более оптимально работать с вложениями (например, получать к ним
         * доступ в шейдере до его записи в кадровый буер) за счет быстрой памяти устройство (своего рода кэш). Но стоит
         * также учитывать ограничения: при таких эффектах как размытие некоторые из тайлов могут быть не готовы, и их
         * данные будут недоступны. Под-проходы идеально подходят для реализации таких концепций как G-буфер, отложенный
         * рендеринг и прочие аналогичные техники. Каждому под-проходу может соответствовать свой отдельный конвейер
         * с шейдерами.
         */

        // Описания вложений.
        // Предполагается использование двух вложений - цвета и глубины/трафарета.
        // Вложение - изображение, в которое производится запись на стороне shader'а.
        // Вложение также может быть прочитано в другом под-проходе (например, для легкой пост-обработки)
        std::vector<::vk::AttachmentDescription> attachment_descriptions{};

        // Цвет
        attachment_descriptions.push_back(
            vk::AttachmentDescription()
            .setFormat(config_.color_format)
            .setSamples(vk::SampleCountFlagBits::e1)                        // Без multisampling (1 семпл)
            .setLoadOp(vk::AttachmentLoadOp::eClear)                        // Очистка вложение в начале под-прохода
            .setStoreOp(vk::AttachmentStoreOp::eStore)                      // Хранить для показа (один под-проход)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)              // Трафарет не используем (цветовое вложение)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)            // Трафарет не используем (цветовое вложение)
            .setInitialLayout(vk::ImageLayout::eUndefined)                  // Изначального макета памяти еще нет
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR)                // В конце - отправка на экран (один под-проход)
        );

        // Глубина/трафарет
        attachment_descriptions.push_back(
            vk::AttachmentDescription()
            .setFormat(config_.depth_stencil_format)
            .setSamples(vk::SampleCountFlagBits::e1)                        // Без multisampling (1 семпл)
            .setLoadOp(vk::AttachmentLoadOp::eClear)                        // Очистка вложение в начале под-прохода
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)                   // Хранить для показа не нужно (не показываем)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)              // Трафарет не используем (только глубина)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)            // Трафарет не используем (только глубина)
            .setInitialLayout(vk::ImageLayout::eUndefined)                  // Изначального макета памяти еще нет
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)); // В конце - использование в качестве вложения глубины/трафарета

        // Ссылки на вложения.
        // Указываем, какие вложения из ранее описанных (и в качестве чего) будет использовать под-проход.
        // На данный момент: 1 проход с 1 под-проходом (с последующим выводом изображения в кадровый буфер).
        std::vector<vk::AttachmentReference> color_attachment_refs{};
        std::vector<vk::AttachmentReference> depth_attachment_refs{};

        color_attachment_refs.push_back(
            vk::AttachmentReference()
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal));

        depth_attachment_refs.push_back(
            vk::AttachmentReference()
            .setAttachment(1)
            .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal));

        // Под-проходы (1 под проход на текущий момент)
        std::vector<vk::SubpassDescription> subpass_descriptions{};
        subpass_descriptions.push_back(
            vk::SubpassDescription().
            setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachments(color_attachment_refs)
            .setPDepthStencilAttachment(&depth_attachment_refs[0]));

        // Зависимости под-проходов.
        // Указываем, на каких стадиях конвейера какой будет доступ к вложениям под-проходов
        std::vector<vk::SubpassDependency> subpass_dependencies{};

        // Переход из внешнего (неявного) в основной (первый/нулевой)
        subpass_dependencies.push_back(
            vk::SubpassDependency()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)                                   // Исходный под-проход (внешний)
            .setDstSubpass(0)                                                     // Целевой (первый)
            .setSrcStageMask(vk::PipelineStageFlagBits::eTopOfPipe)               // Этап ожидания операций
            .setSrcAccessMask(vk::AccessFlagBits::eNone)                          // Нет операций для ожидания (вложение очищается)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)   // Этап выполнения операций целевого под-прохода
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)          // Операции целевого под-прохода
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)                // Синхронизация (по региону)
        );

        // Переход из основного во внешний (неявный)
        subpass_dependencies.push_back(
            vk::SubpassDependency()
            .setSrcSubpass(0)                                                     // Исходный под-проход (первый)
            .setDstSubpass(VK_SUBPASS_EXTERNAL)                                   // Целевой (внешний)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)   // Этап ожидания операций (вывод)
            .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)          // Операции записи
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)   // Этап выполнения операций целевого под-прохода
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead)           // Операции чтения (swap chain)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)                // Синхронизация (по региону)
        );

        // Создать проход
        vk_render_pass_ = vk_device_->logical_device().createRenderPassUnique(
            vk::RenderPassCreateInfo()
            .setAttachments(attachment_descriptions)
            .setSubpasses(subpass_descriptions)
            .setDependencies(subpass_dependencies));
    }

    void Renderer::init_vk_swap_chain(){
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        // Проверка формата поверхности
        const vk::SurfaceFormatKHR surface_format = {config_.color_format, config_.color_space};
        if (!vk_device_->supports_format(surface_format, vk_surface_)){
            throw std::runtime_error("Surface format is not supported by the device");
        }

        // Проверка поддержки нужного кол-ва изображений
        const auto surface_capabilities = vk_device_->physical_device().getSurfaceCapabilitiesKHR(*vk_surface_);
        if (config_.swap_chain_image_count > surface_capabilities.maxImageCount){
            throw std::runtime_error("Swap chain image count is greater than the maximum supported by the device");
        }

        // Проверка поддержки нужного режима представления (показа)
        const auto present_modes = vk_device_->physical_device().getSurfacePresentModesKHR(*vk_surface_);
        if (std::find(present_modes.begin(), present_modes.end(), config_.present_mode) == present_modes.end()){
            throw std::runtime_error("Present mode is not supported by the device");
        }

        // Старый swap-chain (может быть нужен в случае пере-создания)
        const vk::SwapchainKHR old_swap_chain = vk_swap_chain_ ? *vk_swap_chain_ : nullptr;

        // Индексы семейств очередей рендеринга и показа
        // На текущий момент выделяется одна группа очередей с поддержкой обеих команд (одно семейство)
        const std::vector family_indices = {
            vk_device_->queue_group(to<size_t>(CommandGroup::eGraphicsAndPresent)).family_index.value(),
        };

        // Используется ли одно и то же семейство для показа и рендеринга
        bool same_family = true;
        for (const auto& queue_family_index : family_indices){
            if (queue_family_index != family_indices[0]){
                same_family = false;
                break;
            }
        }

        // Инициализация swap chain
        auto create_info = vk::SwapchainCreateInfoKHR()
        .setSurface(vk_surface_.get())
        .setMinImageCount(config_.swap_chain_image_count)
        .setImageFormat(surface_format.format)
        .setImageColorSpace(surface_format.colorSpace)
        .setImageExtent(surface_capabilities.currentExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setImageSharingMode(same_family ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent)
        .setPreTransform(surface_capabilities.currentTransform)
        .setClipped(true)
        .setOldSwapchain(old_swap_chain);

        if (!same_family){
            create_info.setQueueFamilyIndices(family_indices);
        }

        // Если swap chain уже существует - лишить владения указатель
        if (vk_swap_chain_){
            vk_swap_chain_.release();
        }

        // Создать новый swap chain
        vk_swap_chain_ = vk_device_->logical_device().createSwapchainKHRUnique(create_info);

        // Уничтожить старый swap chain
        if (old_swap_chain){
            vk_device_->logical_device().destroySwapchainKHR(old_swap_chain);
        }
    }

    void Renderer::init_vk_framebuffers(){
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);
        assert(vk_swap_chain_);

        // Получить изображения swap chain
        const auto swap_chain_images = vk_device_->logical_device().getSwapchainImagesKHR(*vk_swap_chain_);
        assert(!swap_chain_images.empty());

        // Получить размеры кадрового буфера
        const auto swap_chain_extent = vk_device_->physical_device().getSurfaceCapabilitiesKHR(*vk_surface_).currentExtent;

        // Проход по изображениям swap-chain
        for (const auto& sci : swap_chain_images)
        {
            // Описать вложения кадрового буфера
            std::vector<vk::utils::Framebuffer::AttachmentInfo> attachments{};

            // Вложение цвета (используем изображение из swap-chain)
            vk::utils::Framebuffer::AttachmentInfo color{};
            color.image = sci;
            color.format = config_.color_format;
            color.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
            color.aspect = vk::ImageAspectFlagBits::eColor;
            attachments.push_back(color);

            // Для вложения глубины-трафарета изображения не создано (swap-chain создает только показываемые изображения)
            // НЕ указываем ничего в поле image (оно будет создано внутри кадрового буфера)
            vk::utils::Framebuffer::AttachmentInfo depth{};
            depth.format = config_.depth_stencil_format;
            depth.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
            depth.aspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
            attachments.push_back(depth);

            // Создать и добавить кадровый буфер
            vk_framebuffers_.emplace_back(std::make_unique<vk::utils::Framebuffer>(
                vk_device_,
                vk_render_pass_.get(),
                swap_chain_extent,
                attachments));
        }
    }

    void Renderer::init_vk_descriptor_pool(){
        assert(vk_instance_);
        assert(vk_device_);

        // Описываем размер дескрипторного пула.
        // На данном этапе нас не интересует структура самих дескрипторных наборов.
        // Важно лишь количество дескрипторов конкретного типа, что можно выделить из пула.
        std::array<::vk::DescriptorPoolSize, 2> pool_sizes{
            // Для камеры будет использован статический буфер
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(MAX_CAMERAS),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBufferDynamic)
                .setDescriptorCount(1),
        };

        // Мы можем использовать разные наборы дескрипторов для камеры и для объектов
        // На текущий момент используем 2 набора (камеры + объекты).
        // Для объектов будет создан один большой uniform буфер, который буфер привязываться со смещением
        vk_descriptor_pool_ = vk_device_->logical_device().createDescriptorPoolUnique(
            vk::DescriptorPoolCreateInfo()
            .setMaxSets(2)
            .setPoolSizes(pool_sizes)
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet));
    }

    void Renderer::init_vk_descriptor_set_layouts(){
        assert(vk_instance_);
        assert(vk_device_);

        // Описываем макет дескрипторных наборов.
        // Макет очень сильно зависит от кода shader'а и должен его учитывать.

        // Дескрипторный набор для вида/камеры
        std::array<vk::DescriptorSetLayoutBinding, 1> view_uniform_bindings = {
            {
                vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                .setDescriptorCount(1)
            }
        };

        vk_dset_layout_view_ = vk_device_->logical_device().createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo()
        .setBindings(view_uniform_bindings));

        // Дескрипторный набор для объектов сцены
        std::array<vk::DescriptorSetLayoutBinding, 1> object_uniform_bindings = {
            {
                vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                .setDescriptorCount(1)
            }
        };

        vk_dset_layout_objects_ = vk_device_->logical_device().createDescriptorSetLayoutUnique(
            vk::DescriptorSetLayoutCreateInfo()
            .setBindings(object_uniform_bindings));
    }

    void Renderer::init_vk_uniforms(){
        assert(vk_instance_);
        assert(vk_device_);
        assert(vk_descriptor_pool_);
        assert(vk_dset_layout_view_);
        assert(vk_dset_layout_objects_);

        // Выравнивание для uniform буферов
        const auto alignment = vk_device_->physical_device()
            .getProperties()
            .limits
            .minUniformBufferOffsetAlignment;

        // Дескрипторные наборы
        {
            // Выделить дескрипторный набор для камеры
            vk_device_->logical_device().allocateDescriptorSetsUnique(
                vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(vk_descriptor_pool_.get())
                .setDescriptorSetCount(1)
                .setSetLayouts({*vk_dset_layout_view_}))
            .front()
            .swap(vk_dset_view_);

            // Выделить дескрипторный набор для объектов
            vk_device_->logical_device().allocateDescriptorSetsUnique(
                vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(vk_descriptor_pool_.get())
                .setDescriptorSetCount(1)
                .setSetLayouts({*vk_dset_layout_objects_}))
            .front()
            .swap(vk_dset_objects_);
        }

        // Uniform буферы
        {
            // Выделить uniform буфер для камеры (вид, проекция)
            vk_ubo_view_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(CameraUniforms), alignment),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для объектов сцены
            vk_ubo_objects_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(ObjectUniforms), alignment) * MAX_OBJECTS,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }

        // Связать дескрипторы и буферы
        {
            const std::array<vk::WriteDescriptorSet, 2> writes
            {
                // Камера
                vk::WriteDescriptorSet()
                .setDstSet(vk_dset_view_.get())
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(
                    vk::DescriptorBufferInfo()
                    .setBuffer(vk_ubo_view_->vk_buffer())
                    .setOffset(0)
                    .setRange(size_align(sizeof(CameraUniforms), alignment)))
                ,

                // Объекты
                vk::WriteDescriptorSet()
                .setDstSet(vk_dset_objects_.get())
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
                .setDescriptorCount(1)
                .setBufferInfo(
                    vk::DescriptorBufferInfo()
                    .setBuffer(vk_ubo_objects_->vk_buffer())
                    .setOffset(0)
                    .setRange(size_align(sizeof(ObjectUniforms), alignment))
                )
            };

            vk_device_->logical_device().updateDescriptorSets(writes, {});
        }
    }

    void Renderer::init_vk_command_buffers(){
        assert(vk_instance_);
        assert(vk_device_);

        // Выделяем столько командных буферов, сколько предполагается активных кадров
        auto& pool = vk_device_->queue_group(to<size_t>(CommandGroup::eGraphicsAndPresent)).command_pools[0];
        vk_command_buffers_ = vk_device_->logical_device().allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo()
            .setCommandPool(pool.get())
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(config_.max_frames_in_flight));
    }

    void Renderer::init_vk_sync_objects(){
        assert(vk_instance_);
        assert(vk_device_);

        // Создать необходимые примитивы синхронизации для каждого активного кадра
        const auto& ld = vk_device_->logical_device();
        for (size_t i = 0; i < config_.max_frames_in_flight; ++i)
        {
            // Семафор, который будет ожидаться конвейером перед выполнением команд рендеринга
            vk_render_available_semaphore_.emplace_back(ld.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
            // Семафор, который будет сигнализировать о готовности к показу изображения (для команд показа)
            vk_render_finished_semaphore_.emplace_back(ld.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
            // Барьеры, которые показывают, что буфер был выполнен и готов к использованию
            vk_frame_fence_.emplace_back(ld.createFenceUnique(vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled}));
        }
    }

    void Renderer::refresh_vk_surface(){
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        // Ожидать завершения всех команд
        vk_device_->logical_device().waitIdle();

        // Отключить рендеринг и сбросить кадр
        is_rendering_ = false;
        current_frame_ = 0;

        // Очистить командные буферы
        vk_command_buffers_.clear();
        logger()->info("Vulkan: cleared command buffers");
        
        // Уничтожить кадровые буферы
        vk_framebuffers_.clear();
        logger()->info("Vulkan: cleared framebuffers");

        // Пере-создать swap-chain (старый будет задействован при создании нового, затем удален)
        init_vk_swap_chain();
        logger()->info("Vulkan: recreated swap chain");

        // Создать новые кадровые буферы
        init_vk_framebuffers();
        const auto extent = vk_framebuffers_[0]->extent();
        const std::string extent_str = std::to_string(extent.width) + "x" + std::to_string(extent.height);
        logger()->info("Vulkan: recreated framebuffers (" + extent_str + ")");

        // Создать новые командные буферы
        init_vk_command_buffers();
        logger()->info("Vulkan: recreated command buffers");

        // Включить рендеринг
        is_rendering_ = true;
    }
}
