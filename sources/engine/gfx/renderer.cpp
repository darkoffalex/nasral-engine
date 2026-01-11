#include "pch.h"
#include <nasral/gfx/renderer.h>
#include <nasral/gfx/utils.h>
#include <nasral/evt/utils.h>
#include <nasral/ecs/view.h>
#include <nasral/log/logger.h>
#include <nasral/engine.h>
#include <nasral/res/resources/project.h>

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

    std::optional<ecs::EntityId> Renderer::find_material_entity(const core::UniqueId& uid) const
    {
        using Id = res::comp::AssetId;
        using MatDesc = res::comp::Descriptor;
        using TexDesc = res::comp::DescriptorList<TextureType>;

        for (auto [e, id, md, td] : engine()->ecs()->view<Id, MatDesc, TexDesc>()){
            if (id.uid == uid) return e;
        }

        return std::nullopt;
    }
}

namespace nasral::gfx
{
    Renderer::Renderer(Engine* engine, const Config& config)
        : Subsystem(engine, config)
        , is_active_(false)
        , frame_in_progress_(false)
        , surface_refresh_requested_(false)
        , evt_h_proj_load_(evt::kInvalidListener)
        , evt_h_proj_release_(evt::kInvalidListener)
        , current_frame_(0)
        , available_image_index_(0)
        , object_ids_(kMaxObjects)
        , material_ids_(kMaxMaterials)
        , light_ids_(kMaxLights)
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

        // Регистрация обработки нужных событий
        evt_h_proj_load_ = engine->events()->register_l(
            evt::Type::eProjectResLoaded,
            evt::bind(this, &Renderer::on_project_loaded));

        evt_h_proj_release_ = engine->events()->register_l(
            evt::Type::eProjectResReleasing,
            evt::bind(this, &Renderer::on_project_releasing));

        // Выделение нужного кол-ва памяти
        light_active_ids_.reserve(kMaxLights);
        light_states_.resize(kMaxLights, 0);
        is_active_ = true;
    }

    Renderer::~Renderer()
    {
        // Де-регистрировать обработчики событий
        engine()->events()->unregister_l(evt::Type::eProjectResLoaded, evt_h_proj_load_);
        engine()->events()->unregister_l(evt::Type::eProjectResReleasing, evt_h_proj_release_);

        // Остановить рендеринг и дождаться завершения кадра
        is_active_ = false;
        cmd_wait_for_frame();
    }

#pragma region vk_initialization

    /**
     * @brief Инициализирует Vulkan Instance — корневой объект Vulkan.
     * @details Назначение Instance: регистрация в драйвере, выбор расширений/слоёв и точка входа к Surface/Device.
     */
    void Renderer::init_vk_instance()
    {
        // Требуемые расширения и слои
        std::vector<const char*> req_extensions = config().surface_provider->extensions();
        std::vector<const char*> req_layers = {};

        // Если нужна валидация
        if (config().enable_validation_layers){
            req_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            req_layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        // Информация о приложении
        const auto app_info = vk::ApplicationInfo()
            .setPApplicationName(config().app_name.c_str())
            .setPEngineName(config().engine_name.c_str())
            .setApiVersion(VK_API_VERSION_1_4)
            .setApplicationVersion(kVkAppVersion)
            .setEngineVersion(kVkEngineVersion);

        // Создать сущность Vulkan
        vk_instance_ = vk::createInstanceUnique(vk::InstanceCreateInfo()
            .setPEnabledExtensionNames(req_extensions)
            .setPEnabledLayerNames(req_layers)
            .setPApplicationInfo(&app_info));
    }

    /**
     * @brief Инициализация загрузчика функций Vulkan
     * @details DispatchLoaderDynamic загружает указатели на функции Vulkan в runtime. Это нужно, поскольку
     * многие вызовы (особенно расширения и функции уровня instance/устройства) не доступны статически и
     * должны получаться через vkGetInstanceProcAddr. Здесь мы создаём загрузчик из текущего Instance и
     * функции получения адресов из config_.pfn_vk_get_proc_addr и затем инициализируем его.
     */
    void Renderer::init_vk_loader()
    {
        assert(vk_instance_);
        vk_loader_ = vk::detail::DispatchLoaderDynamic(vk_instance_.get(), config().pfn_vk_get_proc_addr);
        vk_loader_.init(vk_instance_.get());
    }

    /**
     * @brief Включает DebugReportCallback для сообщений от слоёв валидации.
     * @details Слои валидации помогают находить проблемы в runtime во время разработки. Используется только при
     * config_.enable_validation_layers. Создаёт объект vk::DebugReportCallbackEXT с флагами eError|eWarning
     * и callback функцией vk_debug_report_callback. Callback получает предупреждения/ошибки от драйвера и слоёв,
     * а мы передаём engine()->logger() как pUserData для логирования. Требует валидного vk_instance_ и загрузчика
     * vk_loader_ (функции берутся через vkGetInstanceProcAddr).
     */
    void Renderer::init_vk_debug_callback()
    {
        assert(vk_instance_);
        if (!config().enable_validation_layers){
            return;
        }

        // Используем метод vk_debug_report_callback для логирования предупреждений и ошибок от слоев.
        // Передаем engine()->logger() как пользовательский указатель (для логирования).
        const auto info = vk::DebugReportCallbackCreateInfoEXT()
                .setFlags(vk::DebugReportFlagBitsEXT::eError|vk::DebugReportFlagBitsEXT::eWarning)
                .setPfnCallback(reinterpret_cast<vk::PFN_DebugReportCallbackEXT>(vk_debug_report_callback))
                .setPUserData(engine()->logger());

        // Создать debug report callback объект
        vk_debug_callback_ = vk_instance_->createDebugReportCallbackEXTUnique(
            info,
            nullptr,
            vk_loader_);
    }

    /**
     * @brief Инициализирует поверхность вывода (SurfaceKHR) для связки Vulkan с оконной системой.
     * @details Поверхность — WSI-объект, через который изображения из Vulkan показываются в окне.
     * Она создаётся вне renderer (на стороне приложения) через surface_provider->create_surface(*vk_instance_).
     * Здесь мы принимаем «сырой» vk::SurfaceKHR от провайдера и оборачиваем его в vk::UniqueSurfaceKHR,
     * указав корректный удалитель, связанный с текущим vk_instance_. Поверхность далее используется для
     * выбора совместимого устройства/очередей и при создании swap chain.
     */
    void Renderer::init_vk_surface()
    {
        // Поверхность создается вне системы рендеринга (на стороне приложения, например, при помощи GLFW)
        const vk::SurfaceKHR surface = config().surface_provider->create_surface(*vk_instance_);

        // Поскольку был создан "голый" handler, нужно обернуть его в unique pointer
        // Также нужно предоставить функтор удаления (второй аргумент, используем стандартный)
        vk_surface_ = vk::UniqueSurfaceKHR(
            surface,
            ::vk::detail::ObjectDestroy<::vk::Instance, ::vk::detail::DispatchLoaderStatic>(vk_instance_.get()));
    }

    /**
     * @brief Инициализирует логическое устройство через обёртку vk::utils::Device.
     * @details Используется RAII‑обёртка vk::utils::Device: она выбирает совместимое физическое устройство,
     * формирует очереди, включает нужные расширения и создаёт VkDevice. Здесь мы запрашиваем две группы очередей:
     * - eGraphicsAndPresent: 2 графические очереди с показом (рендер и показ + отдельная очередь для загрузок).
     * - eTransfer: 1 очередь переноса для асинхронных копирований/аллокаций.
     * Разделение позволяет загружать/переносить ресурсы, не блокируя выполнение рендеринга и показа.
     * Также передаются требуемые расширения: VK_KHR_swapchain, VK_KHR_dedicated_allocation, EXT_descriptor_indexing.
     */
    void Renderer::init_vk_device()
    {
        assert(vk_instance_);
        assert(vk_surface_);

        // Требуемые расширения (поддержка своп-чейна и выделенных аллокаций памяти)
        const std::vector req_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
        };

        // Требования к очередям команд у устройства
        // Первая группа содержит 2 графические очереди (рендеринг и операции загрузки текстур)
        // Вторая группа содержит 1 очередь команд переноса данных (загрузка ресурсов)
        std::vector<vk::utils::Device::QueueGroupRequest> req_queues(static_cast<size_t>(CommandGroup::TOTAL));
        req_queues[static_cast<size_t>(CommandGroup::eGraphicsAndPresent)] = vk::utils::Device::QueueGroupRequest::graphics(2, true);
        req_queues[static_cast<size_t>(CommandGroup::eTransfer)] = vk::utils::Device::QueueGroupRequest::transfer(1);

        // Создать устройство
        vk_device_ = std::make_unique<vk::utils::Device>(
            vk_instance_,
            vk_surface_,
            req_queues,
            req_extensions);
    }

    /**
     * @brief Инициализация проходов рендеринга.
     * @details В отличие от OpenGL, где проход - абстракция чисто программная, в Vulkan проход это отдельный объект.
     * Он описывает, как ведет себя память вложений (цвет, глубина) на разных этапах конвейера рендеринга. У каждого
     * прохода есть свои под-проходы, они позволяют более оптимально работать с вложениями (например, получать к ним
     * доступ в шейдере до его записи в кадровый буер) за счет быстрой памяти устройство (своего рода кэш). Но стоит
     * также учитывать ограничения: при таких эффектах, как размытие, некоторые tiles могут быть не готовы, и их
     * данные будут недоступны. Под-проходы идеально подходят для реализации таких концепций как G-буфер, отложенный
     * рендеринг и прочие аналогичные техники. Каждому под-проходу может соответствовать свой отдельный конвейер
     * с шейдерами.
     */
    void Renderer::init_vk_render_passes()
    {
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        if (!vk_device_->supports_color(config().color_format, vk_surface_)){
            throw std::runtime_error("Color format is not supported by the device");
        }

        if (!vk_device_->supports_depth(config().depth_format)){
            throw std::runtime_error("Depth format is not supported by the device");
        }

        // Описания вложений.
        // Предполагается использование двух вложений - цвета и глубины/трафарета.
        // Вложение - изображение, в которое производится запись на стороне shader'а.
        // Вложение также может быть прочитано в другом под-проходе (например, для легкой пост-обработки)
        std::vector<::vk::AttachmentDescription> attachment_descriptions{};

        // Цвет
        attachment_descriptions.push_back(
            vk::AttachmentDescription()
            .setFormat(config().color_format)
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
            .setFormat(config().depth_format)
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

    /**
     * @brief Создаёт swap chain — очередь изображений для показа на экране.
     * @details Смысл swap chain: хранит несколько изображений (двойная/тройная буферизация),
     * которые по очереди заполняются renderer'ом и представляются на поверхность.
     * Здесь проверяем поддержку формата/цвет-пространства, нужного present mode и
     * допустимого числа изображений. Далее формируем vk::SwapchainCreateInfoKHR:
     * - setMinImageCount(config_.swap_chain_images) в пределах min/max драйвера;
     * - формат и цвет-пространство берутся из config_ (SurfaceFormatKHR);
     * - размер равен capabilities.currentExtent (размер поверхности);
     * - usage = eColorAttachment, arrayLayers = 1;
     * - sharingMode: eExclusive, либо eConcurrent если разные семейства очередей;
     * - preTransform = currentTransform, clipped = true;
     * - oldSwapchain передаём при пересоздании (для корректного владения ресурсами).
     */
    void Renderer::init_vk_swap_chain()
    {
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        // Проверка формата поверхности
        const vk::SurfaceFormatKHR surface_format = {config().color_format, config().color_space};
        if (!vk_device_->supports_format(surface_format, vk_surface_)){
            throw std::runtime_error("Color format is not supported by the device");
        }

        // Проверка поддержки нужного кол-ва изображений
        const auto surface_capabilities = vk_device_->physical_device().getSurfaceCapabilitiesKHR(*vk_surface_);
        if (config().swap_chain_images > surface_capabilities.maxImageCount){
            throw std::runtime_error("Surface does not support requested number of swap chain images");
        }

        // Проверка поддержки нужного режима представления (показа)
        const auto present_modes = vk_device_->physical_device().getSurfacePresentModesKHR(*vk_surface_);
        if (std::find(present_modes.begin(), present_modes.end(), config().present_mode) == present_modes.end()){
            throw std::runtime_error("Present mode is not supported by the surface");
        }

        // Старый swap-chain (может быть нужен в случае пере-создания)
        const vk::SwapchainKHR old_swap_chain = vk_swap_chain_ ? *vk_swap_chain_ : nullptr;

        // Индексы семейств очередей рендеринга и показа
        // На текущий момент выделяется одна группа очередей с поддержкой обеих команд (одно семейство)
        const std::vector family_indices = {
            vk_device_->queue_group(static_cast<size_t>(CommandGroup::eGraphicsAndPresent)).family_index.value(),
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
        .setMinImageCount(config().swap_chain_images)
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

    /**
     * @brief Инициализирует кадровые буферы (framebuffers).
     * @details Используется RAII‑обёртка vk::utils::Framebuffer. Количество кадровых буферов
     * совпадает с количеством изображений своп‑чейна: на каждое изображение создаётся свой framebuffer.
     * Цветовое вложение ссылается на изображение из swap chain (буфер не владеет им — владельцем
     * является swap chain). Вложение глубины/трафарета создаётся внутри обёртки, так как своп‑чейн
     * предоставляет только показываемые цветовые изображения. Такой подход выбран, потому что рендеринг
     * однопроходный: мы выводим сразу в изображения своп‑чейна без промежуточных буферов.
     */
    void Renderer::init_vk_framebuffers()
    {
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
            color.format = config().color_format;
            color.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
            color.aspect = vk::ImageAspectFlagBits::eColor;
            attachments.push_back(color);

            // Для вложения глубины-трафарета изображения не создано (swap-chain создает только показываемые изображения)
            // НЕ указываем ничего в поле image (оно будет создано внутри кадрового буфера)
            vk::utils::Framebuffer::AttachmentInfo depth{};
            depth.format = config().depth_format;
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

    /**
     * @brief Инициализирует макеты uniform’ов: pipeline layout и descriptor pool.
     * @details Используется обёртка vk::utils::UniformLayout: она создаёт descriptor pool, макеты наборов и
     * pipeline layout. Количество наборов (max_sets) и структура привязок должны соответствовать шейдерам, которые будут
     * использоваться. Pipeline layout далее применяется при создании ресурсов материала (pipeline),
     * чтобы шейдеры знали структуру дескрипторов и пуш‑констант. На текущий момент создаём два варианта макета:
     * - eDummy — для шейдеров без uniform переменных и дескрипторов (пустой pipeline layout).
     * - eBasicRasterization — базовый рендеринг (камера, объекты, материалы, текстуры, свет).
     */
    void Renderer::init_vk_uniform_layouts()
    {
        assert(vk_instance_);
        assert(vk_device_);

        auto& layouts = vk_uniform_layouts_;

        // 1. Для шейдеров, которые не используют uniform блоки вообще
        {
            layouts[UniformLayoutType::eDummy] = std::make_unique<vk::utils::UniformLayout>(vk_device_);
        }

        // 2. Для шейдеров базовой растеризации (камера, объекты, текстуры)
        {
            std::vector<vk::utils::UniformLayout::SetLayoutInfo> set_layouts =
            {
                // set = 0: Camera
                {
                    {
                        {
                            0,
                            1,
                            vk::DescriptorType::eUniformBuffer,
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                        }
                    },
                    1
                },
                // set = 1: Objects
                {
                    {
                        // Матрицы трансформации всех объектов
                        {0,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex},
                    },
                    1
                },
                // set = 2: Material settings
                {
                    {
                        // Параметры Phong материала для всех объектов
                        {0,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment},
                        // Параметры PBR материала для всех объектов
                        {1,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment},
                    },
                    1
                },
                // set = 3: Material textures
                {
                    {
                        // Текстуры albedo/diffuse для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eAlbedoColor),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Текстуры normal для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eNormal),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Текстуры roughness/specular для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eRoughnessOrSpecular),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Текстуры displace для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eHeight),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Текстуры metallic для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eMetallicOrReflection),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Текстуры ambient occlusion для всех материалов (PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eAmbientOcclusion),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Текстуры emission для всех материалов (PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eEmission),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                    },
                    1
                },
                // set = 4: Light sources
                {
                    {
                        // Источники света (параметры источников)
                        {
                            0,
                            1,
                            vk::DescriptorType::eStorageBuffer,
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                        },
                        // Индексы источников
                        {
                            1,
                            1,
                            vk::DescriptorType::eStorageBuffer,
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                        }
                    },
                    1
                }
            };

            // Push-константы layout'а растеризации
            std::vector push_constants{
                // Индекс объекта и индекс используемого материала
                vk::PushConstantRange()
                    .setStageFlags(vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment)
                    .setSize(sizeof(uint32_t) * 2)
                    .setOffset(0)
            };

            // Создать pipeline layout для растеризации
            layouts[UniformLayoutType::eBasicRasterization] = std::make_unique<vk::utils::UniformLayout>(
                vk_device_,
                set_layouts,
                push_constants);
        }
    }

    /**
     * @brief Создаёт текстурные семплеры для материалов.
     * @details Семплеры задают способ выборки текстур (фильтрация, режим адресации, анизотропия) и далее
     * привязываются к текстурным дескрипторам (CombinedImageSampler в set=3 макета eBasicRasterization),
     * чтобы шейдеры могли корректно читать текстуры. Создаём варианты: eNearest, eNearestClamp, eLinear,
     * eLinearClamp, eAnisotropic, eAnisotropicClamp (последние используют возможности анизотропии GPU).
     */
    void Renderer::init_vk_texture_samplers()
    {
        assert(vk_instance_);
        assert(vk_device_);

        auto& samplers = vk_texture_samplers_;
        const auto max_anisotropy = vk_device_->physical_device().getProperties().limits.maxSamplerAnisotropy;
        const bool anisotropy_supported = vk_device_->physical_device().getFeatures().samplerAnisotropy;

        // eNearest
        samplers[TextureSamplerType::eNearest] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eNearest)
                .setMagFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setAnisotropyEnable(false)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eNearestClamp
        samplers[TextureSamplerType::eNearestClamp] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eNearest)
                .setMagFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setAnisotropyEnable(false)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eLinear
        samplers[TextureSamplerType::eLinear] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eLinear)
                .setMagFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setAnisotropyEnable(false)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eLinearClamp
        samplers[TextureSamplerType::eLinearClamp] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eLinear)
                .setMagFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setAnisotropyEnable(false)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eAnisotropic
        samplers[TextureSamplerType::eAnisotropic] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eLinear)
                .setMagFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setAnisotropyEnable(anisotropy_supported)
                .setMaxAnisotropy(anisotropy_supported ? max_anisotropy : 1.0f)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eAnisotropicClamp
        samplers[TextureSamplerType::eAnisotropicClamp] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eLinear)
                .setMagFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setAnisotropyEnable(anisotropy_supported)
                .setMaxAnisotropy(anisotropy_supported ? max_anisotropy : 1.0f)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );
    }

    /**
     * @brief Инициализирует uniform’ы: выделяет наборы, создаёт буферы и связывает их с дескрипторами.
     * @details На базе ранее созданного макета eBasicRasterization (UniformLayout) выделяются нужные наборы
     * дескрипторов: set=0 (камера), set=1 (объекты), set=2 (параметры материалов), set=3 (текстуры),
     * set=4 (источники света). Далее создаются UBO/SSBO с учётом выравнивания устройства и лимитов
     * (камера; трансформации объектов; материалы Phong/PBR; параметры и индексы источников света), после чего
     * буферы привязываются к соответствующим binding’ам выделенных наборов через updateDescriptorSets. В конце буферы
     * подготавливаются к использованию — их память маппится (host‑visible/host‑coherent), чтобы движок мог
     * напрямую обновлять содержимое каждый кадр.
     */
    void Renderer::init_vk_uniforms()
    {
        assert(vk_instance_);
        assert(vk_device_);

        // Получить необходимые объекты (пул, макеты наборов)
        const auto& ul = vk_uniform_layouts_[UniformLayoutType::eBasicRasterization];
        assert(ul);

        // Выделить дескрипторный набор для камеры
        ul->allocate_sets(0,1).front().swap(vk_dset_view_);
        // Выделить дескрипторный набор для uniform-буферов объектов (трансформации)
        ul->allocate_sets(1,1).front().swap(vk_dset_objects_uniforms_);
        // Выделить дескрипторный набор для uniform-буферов материалов (блики, шероховатость и прочее)
        ul->allocate_sets(2,1).front().swap(vk_dset_material_uniforms_);
        // Выделить дескрипторный набор для текстур материалов (по массиву дескрипторов на каждый вид текстур)
        ul->allocate_sets(3,1).front().swap(vk_dset_material_textures_);
        // Выделить дескрипторный набор для источников света
        ul->allocate_sets(4,1).front().swap(vk_dset_light_sources_);

        // Uniform буферы
        {
            // Выравнивание для uniform буферов
            const auto ubo_alignment = vk_device_->physical_device()
                .getProperties()
                .limits
                .minUniformBufferOffsetAlignment;

            // Выравнивание для storage буферов
            const auto sbo_alignment = vk_device_->physical_device()
                .getProperties()
                .limits
                .minStorageBufferOffsetAlignment;

            // Выделить uniform буфер для камеры (вид, проекция)
            vk_ubo_view_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(uniforms::Camera), ubo_alignment),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для трансформаций объектов сцены
            vk_ubo_objects_transforms_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(uniforms::Object), sbo_alignment) * kMaxObjects,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для параметров материала (Blin-Phong)
            vk_ubo_materials_phong_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(uniforms::MaterialPhong), sbo_alignment) * kMaxMaterials,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для параметров материала (PBR)
            vk_ubo_materials_pbr_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(uniforms::MaterialPbr), sbo_alignment) * kMaxMaterials,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для источников света
            vk_ubo_light_sources_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(uniforms::LightSettings), sbo_alignment) * kMaxObjects,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для индексов источников
            vk_ubo_light_indices_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(uniforms::LightIndices), sbo_alignment),
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }

        // Связать дескрипторы и буферы
        std::vector<vk::WriteDescriptorSet> writes;
        std::vector<vk::DescriptorBufferInfo> buffer_infos;
        // Зарезервировать память контейнеров перед использованием!
        buffer_infos.reserve(6);
        writes.reserve(6);

        // Камера (set = 0, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_view_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::Camera)));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_view_.get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Трансформации объектов (set = 1, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_objects_transforms_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::Object) * kMaxObjects));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_objects_uniforms_.get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Параметры Phong материалов (set = 2, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_materials_phong_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::MaterialPhong) * kMaxMaterials));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_material_uniforms_.get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Параметры PBR материалов (set = 2, binding = 1)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_materials_pbr_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::MaterialPbr) * kMaxMaterials));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_material_uniforms_.get())
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Источники света (set = 4, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_light_sources_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::LightSettings) * kMaxLights));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_light_sources_.get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Индексы активных источников света (set = 4, binding = 1)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_light_indices_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::LightIndices)));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_light_sources_.get())
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Связать дескрипторы с буферами
        vk_device_->logical_device().updateDescriptorSets(writes, {});

        // Подготовить uniform буферы к записи (разметка памяти)
        vk_ubo_view_->map_unsafe();
        vk_ubo_objects_transforms_->map_unsafe();
        vk_ubo_materials_phong_->map_unsafe();
        vk_ubo_materials_pbr_->map_unsafe();
        vk_ubo_light_sources_->map_unsafe();
        vk_ubo_light_indices_->map_unsafe();
    }

    /**
     * @brief Инициализирует командные буферы рендеринга.
     * @details Выделяет столько первичных командных буферов, сколько допустимо активных кадров
     * (config_.max_frames_in_flight). Это позволяет каждому кадру иметь свой буфер команд и
     * рендериться независимо от других, не блокируя выполнение при ожидании синхронизации
     * предыдущих кадров. Буферы выделяются из командного пула графической группы
     * (eGraphicsAndPresent).
     */
    void Renderer::init_vk_command_buffers()
    {
        assert(vk_instance_);
        assert(vk_device_);

        // Получить нужный командный пул у устройства
        constexpr auto g_idx = static_cast<size_t>(CommandGroup::eGraphicsAndPresent);
        auto& pool = vk_device_->queue_group(g_idx).command_pools[0];

        // Выделяем столько командных буферов, сколько предполагается активных кадров
        vk_command_buffers_ = vk_device_->logical_device().allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo()
            .setCommandPool(pool.get())
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(config().max_frames_in_flight));
    }

    /**
     * @brief Инициализирует примитивы синхронизации рендеринга.
     * @details Создаёт наборы семафоров и заборов (fence) для каждого активного (in‑flight) кадра —
     * ровно config_.max_frames_in_flight штук. Это позволяет кадрам выполняться независимо: пока GPU
     * обрабатывает один кадр, CPU может записывать команды для следующего, не блокируясь на ожиданиях.
     */
    void Renderer::init_vk_synchronization()
    {
        assert(vk_instance_);
        assert(vk_device_);

        // Создать необходимые примитивы синхронизации для каждого активного кадра
        const auto& ld = vk_device_->logical_device();

        // Per-frame синхронизация
        for (size_t i = 0; i < config().max_frames_in_flight; ++i)
        {
            // Семафор, который будет ожидаться конвейером перед выполнением команд рендеринга
            vk_render_available_semaphore_.emplace_back(ld.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
            // Барьеры, которые показывают, что буфер был выполнен и готов к использованию
            vk_frame_fence_.emplace_back(ld.createFenceUnique(vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled}));
        }

        // Per-swap-chain-image синхронизация
        for (size_t i = 0; i < config().swap_chain_images; ++i)
        {
            // Семафор, который будет сигнализировать о готовности к показу изображения (для команд показа)
            vk_render_finished_semaphore_.emplace_back(ld.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
        }
    }

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
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        // Ожидать завершения всех команд
        vk_device_->logical_device().waitIdle();

        // Отключить рендеринг и сбросить кадр
        is_active_ = false;
        current_frame_ = 0;

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

#pragma endregion

#pragma region render_commands

    void Renderer::cmd_begin_frame()
    {
        // Если поверхность вывода обновилась (размеры и прочее)
        if (surface_refresh_requested_.exchange(false, std::memory_order_acquire)){
            refresh_vk_surface();
        }

        // Если рендеринг деактивирован - выйти
        if (!is_active_) return;

        // Кадр начат
        assert(frame_in_progress_ == false);
        frame_in_progress_ = true;

        // Сброс последнего использованного конвейера перед началом кадра
        vk_last_pipeline_ = VK_NULL_HANDLE;

        // Текущий индекс кадра (от 0 включительно до config_.max_frames_in_flight)
        const auto frame_index = get_frame_index();

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
        if (result == vk::Result::eSuccess)
        {
            // Размеры области рендеринга
            const auto& extent = vk_framebuffers_[available_image_index_]->extent();
            const auto& width = extent.width;
            const auto& height = extent.height;

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
                .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)))
                .setClearValues(clear_values),
                vk::SubpassContents::eInline);
        }
        // Если не удалось, возможно, изменилась поверхность - обновить
        else if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
        {
            request_surface_refresh();
        }
    }

    void Renderer::cmd_end_frame()
    {
        // Если рендеринг деактивирован - выйти
        if (!is_active_) return;

        // Если кадр не был начат - выйти
        assert(frame_in_progress_ == true);
        if (!frame_in_progress_) return;

        // Текущий индекс кадра (от 0 включительно до config_.max_frames_in_flight)
        const auto frame_index = get_frame_index();

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
            vk_render_finished_semaphore_[available_image_index_].get()
        };

        // Стадии, на которых конвейер будет ждать wait_semaphores
        std::array<vk::PipelineStageFlags, 1> wait_stages{
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        // Отправить командные буферы на исполнение
        const auto& group = vk_device_->queue_group(static_cast<size_t>(CommandGroup::eGraphicsAndPresent));
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
            frame_in_progress_ = false;
            request_surface_refresh();
            return;
        }

        // Инкремент счетчика кадров
        current_frame_++;

        // Кадр завершен
        frame_in_progress_ = false;
    }

    void Renderer::cmd_bind_material(const handles::Material& handles, const uint32_t index)
    {
        // Если рендеринг деактивирован - выйти
        if (!is_active_) return;

        // Если кадр не был начат - выйти
        if (!frame_in_progress_) {
            assert(false && "Frame not started.");
            return;
        }

        // Текущий индекс кадра (от 0 включительно до config_.max_frames_in_flight)
        const auto frame_index = get_frame_index();

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame_index];

        // Размеры области рендеринга
        const auto& extent = vk_framebuffers_[frame_index]->extent();
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
        const auto& ul = vk_uniform_layouts_[UniformLayoutType::eBasicRasterization];
        static const auto& pl = ul->vk_pipeline_layout();

        // Передать индекс материала через push constant
        cmd_buffer->pushConstants(
            pl,
            vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(uint32_t),
            &index);

        // Запись команд. Если конвейер сменился - привязать
        if (vk_last_pipeline_ != handles.pipeline){
            cmd_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, handles.pipeline);
        }

        // Обновить последний привязанный конвейер
        vk_last_pipeline_ = handles.pipeline;

        // Запись команд. Привязать динамические состояния
        cmd_buffer->setViewport(0, {viewport});
        cmd_buffer->setScissor(0, {scissor});
    }

    void Renderer::cmd_bind_frame_descriptors()
    {
        // Если рендеринг отключен
        if (!is_active_) return;

        // Если кадр не был начат - выйти
        if (!frame_in_progress_) {
            assert(false && "Frame not started.");
            return;
        }

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[get_frame_index()];

        // Получить макет конвейера
        const auto& ul = vk_uniform_layouts_[UniformLayoutType::eBasicRasterization];
        static const auto& pl = ul->vk_pipeline_layout();

        // Привязать все необходимые дескрипторы
        cmd_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pl, 0,
            {
                vk_dset_view_.get(),
                vk_dset_objects_uniforms_.get(),
                vk_dset_material_uniforms_.get(),
                vk_dset_material_textures_.get(),
                vk_dset_light_sources_.get()
            },
            {});
    }

    void Renderer::cmd_draw_mesh(const handles::Mesh& handles, const uint32_t index)
    {
        // Если рендеринг отключен
        if (!is_active_) return;

        // Если кадр не был начат - выйти
        if (!frame_in_progress_) {
            assert(false && "Frame not started.");
            return;
        }

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[get_frame_index()];

        // Получить макет конвейера
        const auto& ul = vk_uniform_layouts_[UniformLayoutType::eBasicRasterization];
        static const auto& pl = ul->vk_pipeline_layout();

        // Передать индекс объекта через push constant
        cmd_buffer->pushConstants(
            pl,
            vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment,
            sizeof(uint32_t),
            sizeof(uint32_t),
            &index);

        // Запись команд. Привязать геометрию и нарисовать её
        cmd_buffer->bindVertexBuffers(0, {handles.vertex_buffer}, {0});
        cmd_buffer->bindIndexBuffer(handles.index_buffer, 0, vk::IndexType::eUint32);
        cmd_buffer->drawIndexed(handles.index_count, 1, 0, 0, 0);
    }

    void Renderer::cmd_wait_for_frame() const
    {
        vk_device_->logical_device().waitIdle();
    }

    void Renderer::request_surface_refresh()
    {
        surface_refresh_requested_.store(true, std::memory_order_release);
    }

#pragma endregion

#pragma region uniforms

    void Renderer::update_cam_uniforms(const uniforms::Camera& uniforms, const uint32_t index) const
    {
        assert(vk_ubo_view_->is_mapped());
        auto& pd = vk_device_->physical_device();
        vk_ubo_view_->update_mapped(
            ubo_offset<uniforms::Camera>(pd, index),
            aligned_ubo<uniforms::Camera>(pd),
            &uniforms);
    }

    void Renderer::update_obj_uniforms(const uniforms::Object& uniforms, const uint32_t index) const
    {
        assert(vk_ubo_view_->is_mapped());
        auto& pd = vk_device_->physical_device();
        vk_ubo_objects_transforms_->update_mapped(
            sbo_offset<uniforms::Object>(pd, index),
            aligned_sbo<uniforms::Object>(pd),
            &uniforms);
    }

    void Renderer::update_mat_phong_uniforms(const uniforms::MaterialPhong& uniforms, const uint32_t index) const
    {
        assert(vk_ubo_materials_phong_->is_mapped());
        auto& pd = vk_device_->physical_device();
        vk_ubo_materials_phong_->update_mapped(
            sbo_offset<uniforms::MaterialPhong>(pd, index),
            aligned_sbo<uniforms::MaterialPhong>(pd),
            &uniforms);
    }

    void Renderer::update_mat_pbr_uniforms(const uniforms::MaterialPbr& uniforms, const uint32_t index) const
    {
        assert(vk_ubo_materials_pbr_->is_mapped());
        auto& pd = vk_device_->physical_device();
        vk_ubo_materials_pbr_->update_mapped(
            sbo_offset<uniforms::MaterialPbr>(pd, index),
            aligned_sbo<uniforms::MaterialPbr>(pd),
            &uniforms);
    }

    void Renderer::update_mat_textures(const TextureBindingInfo& info, const uint32_t index)
    {
        assert(index < kMaxMaterials);
        assert(info.texture);
        assert(vk_dset_material_textures_);

        const auto& sampler = vk_texture_samplers_[info.sampler_type];
        vk::DescriptorImageInfo image_info{};
        image_info.setSampler(sampler.get())
                  .setImageView(info.texture.image_view)
                  .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::WriteDescriptorSet write{};
        write.setDstSet(vk_dset_material_textures_.get())
             .setDstBinding(static_cast<uint32_t>(info.type))
             .setDstArrayElement(index) // Индекс объекта в массиве дескрипторов
             .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
             .setDescriptorCount(1)
             .setImageInfo(image_info);

        vk_device_->logical_device().updateDescriptorSets({write}, {});
    }

    void Renderer::update_light_uniforms(const uniforms::LightSettings& uniforms, const uint32_t index) const
    {
        assert(vk_ubo_light_sources_->is_mapped());
        auto& pd = vk_device_->physical_device();
        vk_ubo_light_sources_->update_mapped(
            sbo_offset<uniforms::LightSettings>(pd, index),
            aligned_sbo<uniforms::LightSettings>(pd),
            &uniforms);
    }

    void Renderer::update_light_states_unsafe(const std::vector<uint32_t>& ids, const bool active)
    {
        assert(vk_ubo_light_indices_->is_mapped());

        // Обновить таблице состояний источников
        const uint8_t state_val = active ? 1 : 0;
        for (const auto& id : ids){
            if (id < light_states_.size()){
                light_states_[id] = state_val;
            }
        }

        // Пересобрать список активных индексов
        light_active_ids_.clear();
        for (uint32_t i = 0; i < static_cast<uint32_t>(light_states_.size()); ++i){
            if (light_states_[i]){
                light_active_ids_.push_back(i);
            }
        }

        // Обновить GPU storage buffer
        auto* pids = static_cast<uniforms::LightIndices*>(vk_ubo_light_indices_->mapped_ptr());
        pids->count = static_cast<uint32_t>(light_active_ids_.size());
        std::fill_n(pids->indices, kMaxLights, 0);
        std::memcpy(pids->indices, light_active_ids_.data(), light_active_ids_.size() * sizeof(uint32_t));
    }

    void Renderer::update_light_states(const std::vector<uint32_t>& ids, const bool active)
    {
        std::lock_guard lock(light_ids_mutex_);
        update_light_states_unsafe(ids, active);
    }
#pragma endregion

#pragma region handlers

    /**
     * @brief Обработка события загрузки файла проекта
     * @param arg Аргумент события
     */
    void Renderer::on_project_loaded(const evt::Arg& arg) const
    {
        auto* r_ptr = static_cast<res::IResource*>(*std::get_if<evt::ArgPtr>(&arg));
        if (const auto* proj = dynamic_cast<res::Project*>(r_ptr))
        {
            // Пройтись по списку материалов
            assert(proj->status() == res::Status::eLoaded);
            for (const auto& m_io : proj->materials())
            {
                // Проверить доступность ресурса
                auto& io_data = m_io->io_material_data;
                if (!engine()->res()->find(io_data.material_path).has_value()){
                    log_error("Material resource not found in list: " + io_data.material_path);
                    return;
                }

                // Создать Entity для экземпляра материала
                const auto m_entity = engine()->ecs()->spawn();

                // Создать необходимые компоненты для entity
                m_io->unpack_to(m_entity, core::IOStruct::eUFStandard);

                // Запросить ресурсы материала
                engine()->ecs()->add_component<res::comp::Request>(m_entity);
                log_info("Material instance registered [" + io_data.id.to_string() + "|" + io_data.material_path + "]");
            }
        }
    }

    /**
     * @brief Обработка события выгрузки проекта
     * @param arg Аргумент события
     */
    void Renderer::on_project_releasing(const evt::Arg& arg)
    {
        using MatDesc = res::comp::Descriptor;
        using TexDesc = res::comp::DescriptorList<TextureType>;
        using MatSettings = comp::MaterialSettings;
        using MatHandles = comp::MaterialHandles;

        auto* ecs = engine()->ecs();
        auto* res = engine()->res();

        auto* r_ptr = static_cast<res::IResource*>(*std::get_if<evt::ArgPtr>(&arg));
        if (const auto* proj = dynamic_cast<res::Project*>(r_ptr))
        {
            // Пройтись по списку материалов
            assert(proj->status() == res::Status::eLoaded);
            for (auto& m_io : proj->materials())
            {
                auto m_entity = find_material_entity(m_io->io_material_data.id);
                if (m_entity.has_value())
                {
                    assert(ecs->has_component<MatDesc>(*m_entity));
                    assert(ecs->has_component<MatHandles>(*m_entity));
                    assert(ecs->has_component<MatSettings>(*m_entity));

                    const auto& md = ecs->get_component<MatDesc>(m_entity.value());
                    const auto& td = ecs->get_component<TexDesc>(m_entity.value());
                    const auto& ms = ecs->get_component<MatSettings>(m_entity.value());

                    // Если есть компоненты хендлов (ресурс загружен)
                    if (ecs->has_component<MatHandles>(m_entity.value()))
                    {
                        // Освободить ресурс материала
                        if (md.res_id != res::kInvalidResourceId){
                            res->release(md.res_id);
                        }

                        // Освободить ресурс текстуры
                        for (const TextureType tt : magic_enum::enum_values<TextureType>()){
                            if (td.res_ids[tt] != res::kInvalidResourceId){
                                res->release(td.res_ids[tt]);
                            }
                        }
                    }

                    // Вернуть индекс материала в пул
                    material_ids().release(ms.index);

                    // Удалить entity
                    ecs->destroy_deferred(m_entity.value(), [this, id = m_io->io_material_data.id]{
                        log_info("Material instance unregistered [" + id.to_string() + "]");
                    });
                }
            }
        }
    }

#pragma endregion

}
