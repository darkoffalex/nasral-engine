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
            ::vk::detail::ObjectDestroy<::vk::Instance, vk::detail::DispatchLoaderStatic>(vk_instance_.get()));
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
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
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
        const vk::SurfaceFormatKHR surface_format = {config().present_color_format, config().present_color_space};
        if (!vk_device_->surface_supports_format(surface_format, vk_surface_)){
            throw std::runtime_error("Color format is not supported by the device");
        }

        // Проверка поддержки нужного кол-ва изображений
        const auto surface_capabilities = vk_device_->physical_device().getSurfaceCapabilitiesKHR(*vk_surface_);
        uint32_t image_count = config().swap_chain_images;

        const auto min_count = surface_capabilities.minImageCount;
        const auto max_count = std::min(surface_capabilities.maxImageCount, kMaxSwapChainImages);

        if (image_count < min_count){
            image_count = surface_capabilities.minImageCount;
        }
        if (max_count != 0 && image_count > surface_capabilities.maxImageCount){
            image_count = surface_capabilities.maxImageCount;
        }

        // Выбор composite alpha
        if (!(surface_capabilities.supportedCompositeAlpha & config().composite_alpha)){
            throw std::runtime_error("Composite alpha mode is not supported by surface");
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
            .setMinImageCount(image_count)
            .setImageFormat(surface_format.format)
            .setImageColorSpace(surface_format.colorSpace)
            .setImageExtent(vk_device_->clamp_swapchain_extent(config().surface_provider->framebuffer_extent(), *vk_surface_))
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
            .setImageSharingMode(same_family ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent)
            .setPreTransform(surface_capabilities.currentTransform)
            .setCompositeAlpha(config().composite_alpha)
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

        // Убеждаемся, что формат глубины поддерживается
        if (!vk_device_->supports_depth(config().offscreen_depth_format)) {
            throw std::runtime_error("Configured offscreen depth format is not supported");
        }

        // Убеждаемся, что формат глубины поддерживает необходимые фичи (eDepthStencilAttachment, eSampledImage)
        if (!vk_device_->supports_format_tiling_features(config().offscreen_depth_format,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment |
            vk::FormatFeatureFlagBits::eSampledImage))
        {
            throw std::runtime_error("Configured offscreen depth format does not support required attachment sampling");
        }

        // Убеждаемся, что формат цветовых вложений поддерживает необходимые фичи
        if (!vk_device_->supports_format_tiling_features(config().offscreen_color_format,
            vk::FormatFeatureFlagBits::eColorAttachment |
            vk::FormatFeatureFlagBits::eSampledImage |
            vk::FormatFeatureFlagBits::eTransferSrc |
            vk::FormatFeatureFlagBits::eTransferDst |
            vk::FormatFeatureFlagBits::eBlitSrc |
            vk::FormatFeatureFlagBits::eBlitDst |
            vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
        {
            throw std::runtime_error("Configured offscreen color format does not support required attachment sampling");
        }

        // Убеждаемся, что разрешение рендеринга не равно нулю
        const auto rendering_extent = rendering_resolution();
        if (rendering_extent.width == 0 || rendering_extent.height == 0)
        {
            throw std::runtime_error("Rendering and output resolution must be non-zero");
        }

        // Для каждого кадра (in flight) и для каждого типа вложения кадра
        FOREACH_FRAME
        for (const auto type : magic_enum::enum_values<FrameAttachmentType>())
        {
            if (type == FrameAttachmentType::TOTAL) {
                continue;
            }

            // Является ли текущий тип вложения кадра типом глубины
            const bool is_depth = type == FrameAttachmentType::eDepth;
            // Формат вложения кадра
            const auto format = is_depth ? config().offscreen_depth_format : config().offscreen_color_format;
            // Аспект вложения кадра (с каким аспектом будет создаваться view)
            const auto aspect = frame_attachment_aspect(type, format);

            constexpr auto color_usage = vk::ImageUsageFlags(
                vk::ImageUsageFlagBits::eColorAttachment |
                vk::ImageUsageFlagBits::eSampled |
                vk::ImageUsageFlagBits::eTransferSrc |
                vk::ImageUsageFlagBits::eTransferDst);

            constexpr auto depth_usage = vk::ImageUsageFlags(
                vk::ImageUsageFlagBits::eDepthStencilAttachment |
                vk::ImageUsageFlagBits::eSampled);

            // Как используется вложение кадра
            auto usage = is_depth ? depth_usage : color_usage;

            // Создание изображения для вложения кадра
            vk_offscreen_images_[type][FRAME] = std::make_unique<vk::utils::Image>(
                vk_device_.get(),
                vk::utils::Image::Type::e2D,
                format,
                vk::Extent3D(rendering_extent.width, rendering_extent.height, 1),
                usage,
                vk::ImageTiling::eOptimal,
                aspect,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::ImageLayout::eUndefined,
                vk::SampleCountFlagBits::e1,
                is_depth ? 1u : 0);
        }
        END_FOREACH

        // Для каждого изображения (swap chain)
        const auto swap_chain_images = vk_device_->logical_device().getSwapchainImagesKHR(*vk_swap_chain_);
        assert(!swap_chain_images.empty());
        for (size_t i = 0; i < swap_chain_images.size(); ++i)
        {
            // Создание изображения для вложения кадра (на основе изображения swap chain)
            auto& image = swap_chain_images[i];
            vk_swap_chain_images_[i] = std::make_unique<vk::utils::Image>(
                vk_device_.get(),
                vk::utils::Image::Type::e2D,
                image,
                config().present_color_format,
                vk::ImageAspectFlagBits::eColor,
                1u);
        }
    }

    /**
     * @brief Инициализирует макеты uniform’ов: pipeline layout и descriptor pool.
     * @details Используется обёртка vk::utils::UniformLayout: она создаёт descriptor pool, макеты наборов и
     * pipeline layout. Количество наборов (max_sets) и структура привязок должны соответствовать шейдерам, которые будут
     * использоваться. Pipeline layout далее применяется при создании ресурсов шейдерной программы (pipeline). Для разных
     * типов шейдерных программ могут быть разные макеты uniform’ов. На текущий момент создаём для:
     * - eRasterization — для шейдеров рендеринга геометрии через растеризатор
     * - eScreenSpace - для шейдеров пост-обработки (промежуточный проход)
     * - eScreenOutput - для шейдеров пост-обработки (последний проход, вывод на экран)
     */
    void Renderer::init_vk_uniform_layouts()
    {
        // eRasterization
        {
            std::vector<vk::utils::UniformLayout::SetLayoutInfo> set_layouts =
            {
                // set = 0: (eViewUBO)
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
                // set = 1: (eObjectUBOs)
                {
                    {
                        // Матрицы трансформации всех объектов
                        {0,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex},
                    },
                    1
                },
                // set = 2: (eMaterialUBOs)
                {
                    {
                        // Параметры Phong материала для всех объектов
                        {0,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment},
                        // Параметры PBR материала для всех объектов
                        {1,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment},
                    },
                    1
                },
                // set = 3: (eMaterialTextures)
                {
                    {
                        // Текстуры albedo/diffuse для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eAlbedoColor),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound | vk::DescriptorBindingFlagBitsEXT::eUpdateAfterBind
                        },
                        // Текстуры normal для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eNormal),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound | vk::DescriptorBindingFlagBitsEXT::eUpdateAfterBind
                        },
                        // Текстуры roughness/specular для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eRoughOrSpec),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound | vk::DescriptorBindingFlagBitsEXT::eUpdateAfterBind
                        },
                        // Текстуры displace для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eHeight),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound | vk::DescriptorBindingFlagBitsEXT::eUpdateAfterBind
                        },
                        // Текстуры metallic для всех материалов (Phong/PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eMetalOrReflect),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound | vk::DescriptorBindingFlagBitsEXT::eUpdateAfterBind
                        },
                        // Текстуры ambient occlusion для всех материалов (PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eAO),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound | vk::DescriptorBindingFlagBitsEXT::eUpdateAfterBind
                        },
                        // Текстуры emission для всех материалов (PBR материал)
                        {
                            static_cast<uint32_t>(TextureType::eEmission),
                            kMaxMaterials,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound | vk::DescriptorBindingFlagBitsEXT::eUpdateAfterBind
                        },
                    },
                    1
                },
                // set = 4: (eLightUBOs)
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

            // Push-константы layout-а растеризации
            std::vector push_constants{
                // Индекс объекта и индекс используемого материала
                vk::PushConstantRange()
                    .setStageFlags(vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment)
                    .setSize(sizeof(uint32_t) * 2)
                    .setOffset(0)
            };

            // Создать pipeline layout для растеризации
            vk_uniform_layouts_[ShaderProgramType::eRasterization] = std::make_unique<vk::utils::UniformLayout>(
                vk_device_,
                set_layouts,
                push_constants);
        }

        // eScreenSpace
        {
            std::vector<vk::utils::UniformLayout::SetLayoutInfo> set_layouts =
            {
                // set = 0: Текстуры кадровых буферов (eFrameTextures)
                {
                    {
                        // Цвет (итоговый результат рендеринга первого прохода)
                        {
                            static_cast<uint32_t>(FrameAttachmentType::eColor),
                            1,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Глубина кадра
                        {
                            static_cast<uint32_t>(FrameAttachmentType::eDepth),
                            1,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Нормали кадра
                        {
                            static_cast<uint32_t>(FrameAttachmentType::eNormal),
                            1,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Яркие области кадра (сияния)
                        {
                            static_cast<uint32_t>(FrameAttachmentType::eEmission),
                            1,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Промежуточный результат ping-pong прохода (ping)
                        {
                            static_cast<uint32_t>(FrameAttachmentType::eTemp1),
                            1,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        },
                        // Промежуточный результат ping-pong прохода (pong)
                        {
                            static_cast<uint32_t>(FrameAttachmentType::eTemp2),
                            1,
                            vk::DescriptorType::eCombinedImageSampler,
                            vk::ShaderStageFlagBits::eFragment,
                            vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                        }
                    },
                    // Наборов столько, сколько может быть "кадров на лету".
                    config().max_frames_in_flight
                },
                // set = 1: eViewUBO (для трансформаций в пространство вида)
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
                // set = 2: eScreenPassUBOs (Настройки экранных эффектов)
                {
                    {
                        {
                            0,
                            1,
                            vk::DescriptorType::eStorageBuffer,
                            vk::ShaderStageFlagBits::eFragment
                        }
                    },
                    1
                }
            };

            // Push-константы layout-а пост-обработки
            std::vector push_constants{
                // Номер прохода (например, для bilateral blur)
                vk::PushConstantRange()
                    .setStageFlags(vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment)
                    .setSize(sizeof(uint32_t))
                    .setOffset(0)
            };

            // Создать pipeline layout для этапов пост-обработки
            vk_uniform_layouts_[ShaderProgramType::eScreenPass] = std::make_unique<vk::utils::UniformLayout>(
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

    void Renderer::init_vk_framebuffer_bindings()
    {
        // Текстурные семплеры должны быть готовы к этому этапу
        assert(vk_texture_samplers_[TextureSamplerType::eLinear] && "Linear clamp texture sampler is not initialized");
        assert(vk_texture_samplers_[TextureSamplerType::eNearest] && "Nearest texture sampler is not initialized");

        FOREACH_FRAME
        auto& set = vk_screen_d_sets_per_frame_[FRAME];
        std::vector<vk::DescriptorImageInfo> pp_image_infos{};
        std::vector<vk::WriteDescriptorSet> pp_writes{};
        pp_image_infos.reserve(6);
        pp_writes.reserve(6);

        // Цвет (0)
        pp_image_infos.push_back(
            vk::DescriptorImageInfo()
                .setSampler(vk_texture_samplers_[TextureSamplerType::eNearestClamp].get())
                .setImageView(vk_offscreen_images_[FrameAttachmentType::eColor][FRAME]->image_view())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal));

        // Глубина (1)
        pp_image_infos.push_back(
            vk::DescriptorImageInfo()
                .setSampler(vk_texture_samplers_[TextureSamplerType::eNearestClamp].get())
                .setImageView(vk_offscreen_images_[FrameAttachmentType::eDepth][FRAME]->image_view())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal));

        // Нормали (2)
        pp_image_infos.push_back(
            vk::DescriptorImageInfo()
                .setSampler(vk_texture_samplers_[TextureSamplerType::eNearestClamp].get())
                .setImageView(vk_offscreen_images_[FrameAttachmentType::eNormal][FRAME]->image_view())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal));

        // Яркие области (3)
        pp_image_infos.push_back(
            vk::DescriptorImageInfo()
                .setSampler(vk_texture_samplers_[TextureSamplerType::eLinearClamp].get())
                .setImageView(vk_offscreen_images_[FrameAttachmentType::eEmission][FRAME]->image_view())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal));

        // Промежуточный буфер: результат 0 (4 / ping)
        pp_image_infos.push_back(
            vk::DescriptorImageInfo()
                .setSampler(vk_texture_samplers_[TextureSamplerType::eLinearClamp].get())
                .setImageView(vk_offscreen_images_[FrameAttachmentType::eTemp1][FRAME]->image_view())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal));

        // Промежуточный буфер: результат 1 (5 / pong)
        pp_image_infos.push_back(
            vk::DescriptorImageInfo()
                .setSampler(vk_texture_samplers_[TextureSamplerType::eLinearClamp].get())
                .setImageView(vk_offscreen_images_[FrameAttachmentType::eTemp2][FRAME]->image_view())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal));

        // Связать с дескрипторами набора
        pp_writes.push_back(
            vk::WriteDescriptorSet()
                .setDstSet(set.get())
                .setDstBinding(static_cast<uint32_t>(FrameAttachmentType::eColor))
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(pp_image_infos[0]));

        pp_writes.push_back(
            vk::WriteDescriptorSet()
                .setDstSet(set.get())
                .setDstBinding(static_cast<uint32_t>(FrameAttachmentType::eDepth))
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(pp_image_infos[1]));

        pp_writes.push_back(
            vk::WriteDescriptorSet()
                .setDstSet(set.get())
                .setDstBinding(static_cast<uint32_t>(FrameAttachmentType::eNormal))
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(pp_image_infos[2]));

        pp_writes.push_back(
            vk::WriteDescriptorSet()
                .setDstSet(set.get())
                .setDstBinding(static_cast<uint32_t>(FrameAttachmentType::eEmission))
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(pp_image_infos[3]));

        pp_writes.push_back(
            vk::WriteDescriptorSet()
                .setDstSet(set.get())
                .setDstBinding(static_cast<uint32_t>(FrameAttachmentType::eTemp1))
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(pp_image_infos[4]));

        pp_writes.push_back(
            vk::WriteDescriptorSet()
                .setDstSet(set.get())
                .setDstBinding(static_cast<uint32_t>(FrameAttachmentType::eTemp2))
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(pp_image_infos[5]));

        // Обновить набор
        vk_device_->logical_device().updateDescriptorSets(pp_writes, {});
        END_FOREACH
    }

    void Renderer::init_vk_uniforms()
    {
        assert(vk_instance_ && "Vulkan instance is not initialized");
        assert(vk_device_ && "Vulkan device is not initialized");

        // Получить необходимые объекты (пул, макеты наборов)
        const auto& ul_rasterize = vk_uniform_layouts_[ShaderProgramType::eRasterization];
        const auto& ul_screen_pass = vk_uniform_layouts_[ShaderProgramType::eScreenPass];

        assert(ul_rasterize && "Rasterization uniform layout is not initialized");
        assert(ul_screen_pass && "Post-processing uniform layout is not initialized");

        // Выделить дескрипторный набор для камеры (для растеризации и пост-обработки)
        vk_rendering_d_sets_[UniformRasterDSetType::eViewUBO] =
            std::move(ul_rasterize->allocate_sets(static_cast<size_t>(UniformRasterDSetType::eViewUBO), 1).front());
        vk_screen_d_sets_[UniformScreenDSetType::eViewUBO] =
            std::move(ul_screen_pass->allocate_sets(static_cast<size_t>(UniformScreenDSetType::eViewUBO), 1).front());
        // Выделить дескрипторный набор для uniform-буферов настроен экранных эффектов
        vk_screen_d_sets_[UniformScreenDSetType::eScreenPassUBOs] =
            std::move(ul_screen_pass->allocate_sets(static_cast<size_t>(UniformScreenDSetType::eScreenPassUBOs), 1).front());
        // Выделить дескрипторный набор для uniform-буферов объектов (трансформации)
        vk_rendering_d_sets_[UniformRasterDSetType::eObjectUBOs] =
            std::move(ul_rasterize->allocate_sets(static_cast<size_t>(UniformRasterDSetType::eObjectUBOs), 1).front());
        // Выделить дескрипторный набор для uniform-буферов материалов (блики, шероховатость и прочее)
        vk_rendering_d_sets_[UniformRasterDSetType::eMaterialUBOs] =
            std::move(ul_rasterize->allocate_sets(static_cast<size_t>(UniformRasterDSetType::eMaterialUBOs), 1).front());
        // Выделить дескрипторный набор для текстур материалов (по массиву дескрипторов на каждый вид текстур)
        vk_rendering_d_sets_[UniformRasterDSetType::eMaterialTextures] =
            std::move(ul_rasterize->allocate_sets(static_cast<size_t>(UniformRasterDSetType::eMaterialTextures), 1).front());
        // Выделить дескрипторный набор для источников света
        vk_rendering_d_sets_[UniformRasterDSetType::eLightUBOs] =
            std::move(ul_rasterize->allocate_sets(static_cast<size_t>(UniformRasterDSetType::eLightUBOs), 1).front());

        // Выделить дескрипторные наборы пост-процессинга (текстуры кадровых буферов)
        auto pp_sets = ul_screen_pass->allocate_sets(static_cast<size_t>(UniformScreenDSetType::eFrameTextures), config().max_frames_in_flight);
        FOREACH_FRAME
        vk_screen_d_sets_per_frame_[FRAME] = std::move(pp_sets[FRAME]);
        END_FOREACH

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
            vk_uniform_buffers_[UniformBufferType::eMaterials] = std::make_unique<vk::utils::Buffer>(
                vk_device_.get(),
                size_align(max_sizeof<uniforms::Material>(), sbo_alignment) * kMaxMaterials,
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

            // Выделить uniform буфер для настроек экранных эффектов
            vk_uniform_buffers_[UniformBufferType::eScreenPasses] = std::make_unique<vk::utils::Buffer>(
                vk_device_.get(),
                size_align(max_sizeof<uniforms::ScreenPass>(), ubo_alignment),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }

        // TODO: Завершить инициализацию uniform буферов
    }
}
