#include "pch.h"
#include <nasral/gfx/renderer.h>
#include <nasral/engine.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/utils.h>

namespace nasral::gfx
{
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
     * функции получения адресов из config_.pfn_vk_get_proc_addr и затем инициализируем
     */
    void Renderer::init_vk_loader()
    {
        assert(vk_instance_ && "Vulkan instance is not initialized");

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
        assert(vk_instance_ && "Vulkan instance is not initialized");

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
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_surface_ && "Vulkan surface is not initialized");

        // Требуемые расширения (поддержка своп-чейна и выделенных аллокаций памяти)
        const std::vector req_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
        };

        // Требования к очередям команд у устройства
        // Первая группа содержит 2 графические очереди (рендеринг и операции загрузки текстур)
        // Вторая группа содержит 1 очередь команд переноса данных (загрузка ресурсов)
        EnumArray<CmdGroupType, vk::utils::Device::QueueGroupRequest> queue_request;
        queue_request[CmdGroupType::eGraphicsAndPresent] = vk::utils::Device::QueueGroupRequest::graphics(2, true);
        queue_request[CmdGroupType::eTransfer] = vk::utils::Device::QueueGroupRequest::transfer(1);
        auto queue_request_v = to_vec(queue_request, 2);

        // Создать устройство
        vk_device_ = std::make_unique<vk::utils::Device>(
            vk_instance_,
            vk_surface_,
            queue_request_v,
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
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_surface_ && "Vulkan surface is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

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
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_surface_ && "Vulkan surface is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

        // Проверка формата поверхности
        const vk::SurfaceFormatKHR surface_format = {config().color_format, config().color_space};
        if (!vk_device_->supports_format(surface_format, vk_surface_)){
            throw std::runtime_error("Color format is not supported by the device");
        }

        // Проверка поддержки нужного кол-ва изображений
        const auto surface_capabilities = vk_device_->physical_device().getSurfaceCapabilitiesKHR(*vk_surface_);
        uint32_t swap_chain_image_count = config().swap_chain_images;
        if (swap_chain_image_count < surface_capabilities.minImageCount){
            swap_chain_image_count = surface_capabilities.minImageCount;
        }
        if (surface_capabilities.maxImageCount != 0 &&
            swap_chain_image_count > surface_capabilities.maxImageCount)
        {
            swap_chain_image_count = surface_capabilities.maxImageCount;
        }

        // Выбор composite alpha
        auto composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        if (!(surface_capabilities.supportedCompositeAlpha & composite_alpha))
        {
            if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied){
                composite_alpha = vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
            }
            else if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied){
                composite_alpha = vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
            }
            else if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit){
                composite_alpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
            }
            else{
                throw std::runtime_error("Surface does not support any known composite alpha mode");
            }
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
            vk_device_->queue_group(static_cast<size_t>(CmdGroupType::eGraphicsAndPresent)).family_index.value(),
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
        .setPresentMode(config().present_mode)
        .setMinImageCount(swap_chain_image_count)
        .setImageFormat(surface_format.format)
        .setImageColorSpace(surface_format.colorSpace)
        .setImageExtent(vk_device_->clamp_swapchain_extent(config().surface_provider->framebuffer_extent(), *vk_surface_))
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setImageSharingMode(same_family ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent)
        .setPreTransform(surface_capabilities.currentTransform)
        .setCompositeAlpha(composite_alpha)
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

    void Renderer::init_vk_framebuffers()
    {
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_surface_ && "Vulkan surface is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");
        assert(vk_swap_chain_ && "Vulkan swap chain is not initialized");

        // Получить изображения swap chain
        const auto swap_chain_images = vk_device_->logical_device().getSwapchainImagesKHR(*vk_swap_chain_);
        assert(!swap_chain_images.empty());

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
                vk_device_.get(),
                vk_render_pass_.get(),
                vk_device_->clamp_swapchain_extent(config().surface_provider->framebuffer_extent(), *vk_surface_),
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
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

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
                            static_cast<uint32_t>(TextureType::eRoughOrSpec),
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
                            static_cast<uint32_t>(TextureType::eMetalOrReflect),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Текстуры ambient occlusion для всех материалов (PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eAO),
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
            layouts[UniformLayoutType::eRasterization] = std::make_unique<vk::utils::UniformLayout>(
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
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

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
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

        // Получить необходимые объекты (пул, макеты наборов)
        const auto& ul = vk_uniform_layouts_[UniformLayoutType::eRasterization];
        assert(ul && "Rasterization uniform layout is not initialized");

        // Выделить дескрипторный набор для камеры
        ul->allocate_sets(0,1).front().swap(vk_descriptor_sets_[UniformDSetType::eViewUBO]);
        // Выделить дескрипторный набор для uniform-буферов объектов (трансформации)
        ul->allocate_sets(1,1).front().swap(vk_descriptor_sets_[UniformDSetType::eObjectUBOs]);
        // Выделить дескрипторный набор для uniform-буферов материалов (блики, шероховатость и прочее)
        ul->allocate_sets(2,1).front().swap(vk_descriptor_sets_[UniformDSetType::eMaterialUBOs]);
        // Выделить дескрипторный набор для текстур материалов (по массиву дескрипторов на каждый вид текстур)
        ul->allocate_sets(3,1).front().swap(vk_descriptor_sets_[UniformDSetType::eMaterialTextures]);
        // Выделить дескрипторный набор для источников света
        ul->allocate_sets(4,1).front().swap(vk_descriptor_sets_[UniformDSetType::eLightUBOs]);

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
            vk_uniform_buffers_[UniformBufferType::eView] = std::make_unique<vk::utils::Buffer>(
                vk_device_.get(),
                size_align(sizeof(uniforms::Camera), ubo_alignment) * kMaxCameras,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для трансформаций объектов сцены
            vk_uniform_buffers_[UniformBufferType::eObjects] = std::make_unique<vk::utils::Buffer>(
                vk_device_.get(),
                size_align(sizeof(uniforms::Object), sbo_alignment) * kMaxObjects,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для параметров материала (Blin-Phong)
            vk_uniform_buffers_[UniformBufferType::eMaterialsPhong] = std::make_unique<vk::utils::Buffer>(
                vk_device_.get(),
                size_align(sizeof(uniforms::MaterialPhong), sbo_alignment) * kMaxMaterials,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для параметров материала (PBR)
            vk_uniform_buffers_[UniformBufferType::eMaterialsPBR] = std::make_unique<vk::utils::Buffer>(
                vk_device_.get(),
                size_align(sizeof(uniforms::MaterialPbr), sbo_alignment) * kMaxMaterials,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для источников света
            vk_uniform_buffers_[UniformBufferType::eLightSources] = std::make_unique<vk::utils::Buffer>(
                vk_device_.get(),
                size_align(sizeof(uniforms::LightSettings), sbo_alignment) * kMaxObjects,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для индексов источников
            vk_uniform_buffers_[UniformBufferType::eLightSourcesActive] = std::make_unique<vk::utils::Buffer>(
                vk_device_.get(),
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
                .setBuffer(vk_uniform_buffers_[UniformBufferType::eView]->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::Camera)));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_descriptor_sets_[UniformDSetType::eViewUBO].get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Трансформации объектов (set = 1, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_uniform_buffers_[UniformBufferType::eObjects]->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::Object) * kMaxObjects));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_descriptor_sets_[UniformDSetType::eObjectUBOs].get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Параметры Phong материалов (set = 2, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_uniform_buffers_[UniformBufferType::eMaterialsPhong]->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::MaterialPhong) * kMaxMaterials));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_descriptor_sets_[UniformDSetType::eMaterialUBOs].get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Параметры PBR материалов (set = 2, binding = 1)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_uniform_buffers_[UniformBufferType::eMaterialsPBR]->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::MaterialPbr) * kMaxMaterials));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_descriptor_sets_[UniformDSetType::eMaterialUBOs].get())
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Источники света (set = 4, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_uniform_buffers_[UniformBufferType::eLightSources]->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::LightSettings) * kMaxLights));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_descriptor_sets_[UniformDSetType::eLightUBOs].get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Индексы активных источников света (set = 4, binding = 1)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_uniform_buffers_[UniformBufferType::eLightSourcesActive]->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(uniforms::LightIndices)));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_descriptor_sets_[UniformDSetType::eLightUBOs].get())
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Связать дескрипторы с буферами
        vk_device_->logical_device().updateDescriptorSets(writes, {});

        // Подготовить uniform буферы к записи (разметка памяти)
        vk_uniform_buffers_[UniformBufferType::eView]->map_unsafe();
        vk_uniform_buffers_[UniformBufferType::eObjects]->map_unsafe();
        vk_uniform_buffers_[UniformBufferType::eMaterialsPhong]->map_unsafe();
        vk_uniform_buffers_[UniformBufferType::eMaterialsPBR]->map_unsafe();
        vk_uniform_buffers_[UniformBufferType::eLightSources]->map_unsafe();
        vk_uniform_buffers_[UniformBufferType::eLightSourcesActive]->map_unsafe();
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
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

        // Получить нужный командный пул у устройства
        constexpr auto g_idx = static_cast<size_t>(CmdGroupType::eGraphicsAndPresent);
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
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

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
}
