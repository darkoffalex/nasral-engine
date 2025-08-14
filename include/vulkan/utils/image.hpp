/**
 * @file image.hpp
 * @brief RAII обертка для управления изображениями Vulkan
 * @author Alex "DarkWolf" Nem
 * @date 2025
 * @copyright MIT License
 *
 * Файл содержит реализацию класса Image, который инкапсулирует работу
 * с изображениями Vulkan, включая их создание, управление памятью и представлениями.
 */

#pragma once
#include <vector>
#include <cmath>
#include <vulkan/vulkan.hpp>
#include <vulkan/utils/device.hpp>

namespace vk::utils
{
    /**
     * @brief RAII обертка для управления изображениями Vulkan.
     *
     * Предоставляет высокоуровневый интерфейс для:
     *  создания изображений различных типов (1D, 2D, 3D, кубических),
     *  управления памятью изображений,
     *  работы с представлениями изображений (image views),
     *  поддержки мип-уровней и массивов слоев
     */
    class Image
    {
    public:
        typedef std::unique_ptr<Image> Ptr;

        /**
         * @brief Тип изображения.
         *
         * Определяет базовую размерность и организацию изображения
         */
        enum class Type : uint16_t
        {
            e1D = 0,        ///< Одномерное изображение
            e2D,            ///< Двумерное изображение
            e2DArray,       ///< Массив двумерных изображений
            eCube,          ///< Кубическое изображение (6 граней)
            eCubeArray,     ///< Массив кубических изображений
            e3D             ///< Трехмерное изображение
        };

        /** @brief Конструктор по умолчанию */
        Image():
            vk_device_(nullptr),
            image_(nullptr),
            image_view_(nullptr),
            own_image_(nullptr),
            own_image_memory_(nullptr),
            mip_levels_(1)
        {}

        /**
         * @brief Создает новое изображение с выделением памяти
         * @param device Устройство Vulkan (обертка из utils)
         * @param type Тип создаваемого изображения
         * @param format Формат пикселей изображения
         * @param extent Размеры изображения
         * @param usage Флаги использования изображения (текстура, вложение фрейм-буфера и прочее)
         * @param tiling Способ размещения данных в памяти
         * @param aspect Аспект(ы) для создания представления (как интерпретировать при доступе - цвет/глубина/трафарет)
         * @param memory_properties Требуемые свойства выделяемой памяти (память устройства (GPU) или хоста (CPU))
         * @param initial_layout Начальный layout изображения
         * @param samples Кол-во sample'ов на тексель (используется при multi-sampling'е)
         * @param mip_levels Количество уровней мип-уровней (0 для автоматического расчета)
         * @param array_layers Количество слоев для массивов изображений
         * @param queue_family_indices Индексы семейств очередей для shared/concurrent доступа
         * @throw std::runtime_error При ошибках создания или выделения памяти
         */
        Image(const Device::Ptr& device,
              const Type& type,
              const vk::Format& format,
              const vk::Extent3D& extent,
              const vk::ImageUsageFlags& usage,
              const vk::ImageTiling& tiling,
              const vk::ImageAspectFlags& aspect,
              const vk::MemoryPropertyFlags& memory_properties,
              const vk::ImageLayout& initial_layout = vk::ImageLayout::eUndefined,
              const vk::SampleCountFlagBits& samples = vk::SampleCountFlagBits::e1,
              const uint32_t mip_levels = 1u,
              const uint32_t array_layers = 1u,
              const std::vector<uint32_t>& queue_family_indices = {})
        : vk_device_(device->logical_device())
        , image_(nullptr)
        , image_view_(nullptr)
        , own_image_(nullptr)
        , own_image_memory_(nullptr)
        {
            assert(device);
            assert(array_layers > 0);
            assert(extent.width > 0 && extent.height > 0 && extent.depth > 0);

            if(type == Type::eCube && array_layers != 6){
                throw std::runtime_error("Array layers must be 6 for cube images");
            }

            if(type == Type::eCubeArray && array_layers % 6 != 0){
                throw std::runtime_error("Array layers must be multiple of 6 for cube array images");
            }

            if(type == Type::eCubeArray && !device->physical_device().getFeatures().imageCubeArray){
                throw std::runtime_error("Device does not support cube array images");
            }

            if(type == Type::eCube && extent.width != extent.height){
                throw std::runtime_error("Cube images must have equal width and height");
            }

            // Задать или определить автоматически число мип-уровней
            if(mip_levels > 0){
                mip_levels_ = mip_levels;
            }else{
                mip_levels_ = static_cast<uint32_t>(std::floor(std::log2((std::max)(extent.width, extent.height)))) + 1;
            }

            // Режим доступа (будут ли разные семейства обращаться к изображению)
            const auto sharing_mode = queue_family_indices.size() > 1 ?
                    vk::SharingMode::eConcurrent :
                    vk::SharingMode::eExclusive;

            auto create_flags = vk::ImageCreateFlags();
            if(type == Type::eCube || type == Type::eCubeArray){
                create_flags |= vk::ImageCreateFlagBits::eCubeCompatible;
            }

            // Создать изображение
            own_image_ = device->logical_device().createImageUnique(
                    ImageCreateInfo()
                    .setImageType(vk_image_type(type))
                    .setFormat(format)
                    .setExtent(extent)
                    .setUsage(usage)
                    .setSamples(samples)
                    .setTiling(tiling)
                    .setInitialLayout(initial_layout)
                    .setMipLevels(mip_levels_)
                    .setArrayLayers(array_layers)
                    .setSharingMode(sharing_mode)
                    .setQueueFamilyIndices(queue_family_indices)
                    .setFlags(create_flags));

            // Свойства памяти
            const auto mem_reqs = device->logical_device().getImageMemoryRequirements(own_image_.get());
            const auto mem_type_index = device->find_memory_type_index(mem_reqs, memory_properties);
            if(!mem_type_index.has_value()){
                throw std::runtime_error("Failed to find suitable memory type for image");
            }

            // Попытка выделения памяти
            try{
                // Выделить память устройства
                own_image_memory_ = device->logical_device().allocateMemoryUnique(
                        MemoryAllocateInfo()
                        .setAllocationSize(mem_reqs.size)
                        .setMemoryTypeIndex(mem_type_index.value()));

                // Связать память и изображения
                device->logical_device().bindImageMemory(own_image_.get(), own_image_memory_.get(), 0);
            }
            catch(const ::vk::OutOfDeviceMemoryError& e) {
                own_image_.reset();
                own_image_memory_.reset();
                throw std::runtime_error("Failed to allocate image memory. " + std::string(e.what()));
            }
            catch(const ::vk::OutOfHostMemoryError& e) {
                own_image_.reset();
                own_image_memory_.reset();
                throw std::runtime_error("Failed to allocate image memory. " + std::string(e.what()));
            }
            catch(const std::exception& e) {
                own_image_.reset();
                own_image_memory_.reset();
                throw std::runtime_error("Failed to allocate image memory. " + std::string(e.what()));
            }

            // Если изображение используется только для как источник копирования - нам не нужен image view
            if (usage == vk::ImageUsageFlagBits::eTransferSrc){
                return;
            }

            // Создать объект image-vew (для доступа к памяти изображения)
            image_view_ = device->logical_device().createImageViewUnique(
                    ImageViewCreateInfo()
                    .setImage(own_image_.get())
                    .setViewType(vk_image_view_type(type))
                    .setFormat(format)
                    .setSubresourceRange(
                            ImageSubresourceRange()
                            .setAspectMask(aspect)
                            .setBaseMipLevel(0)
                            .setLevelCount(mip_levels_)
                            .setBaseArrayLayer(0)
                            .setLayerCount(type == Type::eCube ? 6 : 1)));

        }

        /**
         * @brief Создает представление для существующего изображения
         * @param device Устройство Vulkan (обертка из utils)
         * @param type Тип изображения
         * @param image Существующее изображение Vulkan
         * @param format Формат пикселей изображения
         * @param aspect Аспект(ы) для создания представления (как интерпретировать при доступе - цвет/глубина/трафарет)
         * @param mip_levels Количество мип-уровней
         */
        Image(const Device::Ptr& device,
              const Type& type,
              const vk::Image& image,
              const vk::Format& format,
              const vk::ImageAspectFlags& aspect,
              const uint32_t mip_levels = 1u)
              : Image()
        {
            // Обычно у изображений swap-chain 1 мип-уровень
            mip_levels_ = mip_levels;

            // Копирование handle'а
            image_ = image;

            // Создать объект image-vew (для доступа к памяти изображения)
            image_view_ = device->logical_device().createImageViewUnique(
                    ImageViewCreateInfo()
                    .setImage(image)
                    .setViewType(vk_image_view_type(type))
                    .setFormat(format)
                    .setSubresourceRange(
                            ImageSubresourceRange()
                            .setAspectMask(aspect)
                            .setBaseMipLevel(0)
                            .setLevelCount(mip_levels_)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1)
                    )
            );
        }

        /** @brief Деструктор */
        ~Image() = default;

        /** @brief Запрет копирования */
        Image(const Image&) = delete;

        /** @brief Запрет присваивания */
        Image& operator=(const Image&) = delete;

        /**
         * @brief Возвращает представление изображения
         * @return Константная ссылка на представление изображения
         */
        [[nodiscard]] const vk::ImageView& image_view() const {
            return image_view_.get();
        }

        /**
         * @brief Возвращает handle изображения
         * @return Константная ссылка на изображение
         */
        [[nodiscard]] const vk::Image& image() const {
            return own_image_ ? own_image_.get() : image_;
        }

        /**
         * @brief Возвращает память изображения
         * @return Константная ссылка на память устройства
         */
        [[nodiscard]] const vk::DeviceMemory& memory() const {
            return own_image_memory_.get();
        }

        /**
         * @brief Получить мип-уровни
         * @return Количество мир-уровней
         */
        [[nodiscard]] uint32_t mip_levels() const{
            return mip_levels_;
        }

        /**
         * @brief Отображает (maps) память изображения в память хоста, чтобы можно было изменить данные.
         * @warning Изображение должно быть с MemoryPropertyFlagBits::eHostVisible и vk::ImageLayout::ePreinitialized
         * @param aspect Аспект изображения для разметки (цвет, глубина и прочее)
         * @param layer Слой изображения
         * @param level Мип-уровень изображения
         * @return Указатель на размеченные данные
         */
        void* map(const vk::ImageAspectFlags& aspect, const uint32_t layer = 0, const uint32_t level = 0){
            assert(own_image_memory_);
            assert(own_image_);
            assert(vk_device_);

            const auto isl = vk_device_.getImageSubresourceLayout(
                own_image_.get(),
                {aspect, level, layer});

            void* data = nullptr;
            const auto result = vk_device_.mapMemory(own_image_memory_.get(), isl.offset, isl.size, {}, &data);
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("Failed to map image memory");
            }
            return data;
        }

        /**
         * @brief Снимает отображение (unmaps) памяти изображения.
         */
        void unmap(){
            assert(own_image_memory_);
            assert(own_image_);
            assert(vk_device_);
            vk_device_.unmapMemory(own_image_memory_.get());
        }

        /**
         * @brief Копирование данных в другое Vulkan изображение
         * @param dst_image Целевое изображение (должно быть создано с eTransferSrc и  eHostVisible | eHostCoherent)
         * @param queue_group Группа очередей устройства с поддержкой команд копирования/перемещения
         * @param extent Размер копируемой области
         * @param src_aspect Аспект(ы) для копирования исходного изображения
         * @param dst_aspect Аспект(ы) целевого изображения
         * @param src_layer_count Кол-во копируемых слоев исходного изображения (не путать с mip)
         * @param dst_layer_count Кол-во слоев у целевого изображения
         * @param prepare_for_sampling Подготовить для sampling'а в shader'ах
         */
        void copy_to(const Image& dst_image
                     , Device::QueueGroup& queue_group
                     , const vk::Extent3D& extent
                     , const vk::ImageAspectFlags& src_aspect = vk::ImageAspectFlagBits::eColor
                     , const vk::ImageAspectFlags& dst_aspect = vk::ImageAspectFlagBits::eColor
                     , const uint32_t src_layer_count = 1
                     , const uint32_t dst_layer_count = 1
                     , const bool prepare_for_sampling = true) const
        {
            assert(vk_device_);
            assert(image_ || own_image_);
            assert(dst_image.image() && dst_image.image_view());
            assert(!queue_group.queues.empty() && !queue_group.command_pools.empty());

            // Командный пул и очередь для копирования
            const auto& pool = queue_group.command_pools.back();
            const auto& queue = queue_group.queues.back();
            auto& queue_mutex = queue_group.queue_mutexes.back();

            // Забор команд (для ожидания выполнения нужной команды)
            vk::UniqueFence fence{};
            // Командный буфер (выделяется из пула)
            vk::UniqueCommandBuffer cmd_buffer{};

            // Критическая секция (возможен многопоточный доступ к пулу и очереди)
            {
                std::lock_guard<std::mutex> lock(queue_mutex);

                // Выделить командный буфер
                vk_device_.allocateCommandBuffersUnique(
                    vk::CommandBufferAllocateInfo()
                        .setCommandBufferCount(1)
                        .setCommandPool(pool.get())
                        .setLevel(vk::CommandBufferLevel::ePrimary))
                .back()
                .swap(cmd_buffer);

                // Начать запись команд
                cmd_buffer->begin(
                    vk::CommandBufferBeginInfo()
                        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

                // 1. Барьер: Перевести исходное изображение в eTransferSrcOptimal
                vk::ImageMemoryBarrier src_barrier{};
                src_barrier.setImage(image())
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setOldLayout(vk::ImageLayout::ePreinitialized)
                    .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                    .setSubresourceRange(vk::ImageSubresourceRange()
                        .setAspectMask(src_aspect)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(src_layer_count));

                // 2. Барьер: Перевести целевое изображение в eTransferDstOptimal
                vk::ImageMemoryBarrier dst_barrier{};
                dst_barrier.setImage(dst_image.image())
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setOldLayout(vk::ImageLayout::ePreinitialized)
                    .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits::eNone)
                    .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                    .setSubresourceRange(vk::ImageSubresourceRange()
                        .setAspectMask(dst_aspect)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(dst_layer_count));

                // Применить барьеры для обоих изображений
                const std::array<vk::ImageMemoryBarrier, 2> barriers = {src_barrier, dst_barrier};
                cmd_buffer->pipelineBarrier(
                    vk::PipelineStageFlagBits::eAllCommands,
                    vk::PipelineStageFlagBits::eTransfer,
                    {}, 0, nullptr, 0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data());


                // 3. Копирование данных из исходного изображения в целевое
                vk::ImageCopy copy_region{};
                copy_region.setSrcSubresource(
                    vk::ImageSubresourceLayers()
                        .setAspectMask(src_aspect)
                        .setMipLevel(0)
                        .setBaseArrayLayer(0)
                        .setLayerCount(src_layer_count))
                    .setDstSubresource(
                        vk::ImageSubresourceLayers()
                            .setAspectMask(dst_aspect)
                            .setMipLevel(0)
                            .setBaseArrayLayer(0)
                            .setLayerCount(dst_layer_count))
                    .setSrcOffset({0, 0, 0})
                    .setDstOffset({0, 0, 0})
                    .setExtent(extent);

                cmd_buffer->copyImage(
                    image(),
                    vk::ImageLayout::eTransferSrcOptimal,
                    dst_image.image(),
                    vk::ImageLayout::eTransferDstOptimal,
                    1, &copy_region);

                // 4. Барьер: Перевести целевое изображение в eShaderReadOnlyOptimal (если prepare_for_sampling)
                if (prepare_for_sampling) {
                    dst_barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                        .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

                    cmd_buffer->pipelineBarrier(
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        {}, 0, nullptr, 0, nullptr, 1, &dst_barrier);
                }

                // Завершить запись команд
                cmd_buffer->end();

                // Забор для ожидания выполнения
                fence = vk_device_.createFenceUnique(vk::FenceCreateInfo());

                // Отправить команды в очередь
                queue.submit(vk::SubmitInfo().setCommandBuffers({cmd_buffer.get()}), fence.get());
            }

            // Подождать выполнения
            (void)vk_device_.waitForFences(
                {fence.get()},
                true,
                std::numeric_limits<uint64_t>::max());

            // Критическая секция (возможен многопоточный доступ к пулу)
            {
                std::lock_guard lock(queue_mutex);
                cmd_buffer.reset();
            }
        }

        /**
         * Генерация мип-уровней для изображения
         * @warning Изображение должно быть создано с мип-уровнями
         * @param queue_group Группа очередей устройства с поддержкой команд копирования/перемещения
         * @param initial_extent Изначальное разрешение первого слоя
         * @param aspect Аспект изображения для разметки (цвет, глубина и прочее)
         * @param layer_count Кол-во слоев
         */
        void generate_mipmaps(Device::QueueGroup& queue_group
                              , const vk::Extent3D& initial_extent
                              , const vk::ImageAspectFlags& aspect
                              , const uint32_t layer_count) const
        {
            assert(vk_device_);
            assert(own_image_);
            assert(mip_levels_ > 1);
            assert(!queue_group.queues.empty() && !queue_group.command_pools.empty());

            // Командный пул и очередь для копирования
            const auto& pool = queue_group.command_pools.back();
            const auto& queue = queue_group.queues.back();
            auto& queue_mutex = queue_group.queue_mutexes.back();

            // Забор команд (для ожидания выполнения нужной команды)
            vk::UniqueFence fence{};
            // Командный буфер (выделяется из пула)
            vk::UniqueCommandBuffer cmd_buffer{};

            // Критическая секция (возможен многопоточный доступ к пулу и очереди)
            {
                std::lock_guard<std::mutex> lock(queue_mutex);

                // Выделить командный буфер
                vk_device_.allocateCommandBuffersUnique(
                    vk::CommandBufferAllocateInfo()
                        .setCommandBufferCount(1)
                        .setCommandPool(pool.get())
                        .setLevel(vk::CommandBufferLevel::ePrimary))[0].swap(cmd_buffer);

                // Начать запись команд
                cmd_buffer->begin(
                    vk::CommandBufferBeginInfo()
                        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

                // Предполагается, что базовый мип-уровень (0) уже заполнен данными и находится в eTransferSrcOptimal
                vk::ImageMemoryBarrier barrier{};
                barrier.setImage(image())
                    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(aspect)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(layer_count));

                // Перевести базовый мип-уровень в eTransferSrcOptimal (если не сделано ранее)
                barrier.setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eTransferRead);

                cmd_buffer->pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eTransfer,
                    {}, 0, nullptr, 0, nullptr, 1, &barrier);

                // Цикл по мип-уровням
                vk::Extent2D current_extent{initial_extent.width, initial_extent.height};
                for (uint32_t mip_level = 1; mip_level < mip_levels_; ++mip_level)
                {
                    // Вычислить размеры следующего мип-уровня
                    const vk::Extent2D next_extent{
                        std::max(1u, current_extent.width / 2),
                        std::max(1u, current_extent.height / 2)
                    };

                    // 1. Перевести следующий мип-уровень в eTransferDstOptimal
                    barrier.setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(aspect)
                            .setBaseMipLevel(mip_level)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(layer_count))
                        .setOldLayout(vk::ImageLayout::eUndefined)
                        .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                        .setSrcAccessMask(vk::AccessFlagBits::eNone)
                        .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

                    cmd_buffer->pipelineBarrier(
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, 0, nullptr, 0, nullptr, 1, &barrier);

                    // 2. Выполнить blit из предыдущего мип-уровня в текущий
                    vk::ImageBlit blit_region{};
                    blit_region.setSrcSubresource(
                            vk::ImageSubresourceLayers()
                                .setAspectMask(aspect)
                                .setMipLevel(mip_level - 1)
                                .setBaseArrayLayer(0)
                                .setLayerCount(layer_count))
                        .setSrcOffsets({
                            vk::Offset3D{0, 0, 0},
                            vk::Offset3D{static_cast<int32_t>(current_extent.width)
                                , static_cast<int32_t>(current_extent.height)
                                , 1}
                        })
                        .setDstSubresource(
                            vk::ImageSubresourceLayers()
                                .setAspectMask(aspect)
                                .setMipLevel(mip_level)
                                .setBaseArrayLayer(0)
                                .setLayerCount(layer_count))
                        .setDstOffsets({
                            vk::Offset3D{0, 0, 0},
                            vk::Offset3D{static_cast<int32_t>(next_extent.width)
                                , static_cast<int32_t>(next_extent.height)
                                , 1}
                        });

                    cmd_buffer->blitImage(
                        image(), vk::ImageLayout::eTransferSrcOptimal,
                        image(), vk::ImageLayout::eTransferDstOptimal,
                        1, &blit_region, vk::Filter::eLinear);

                    // 3. Перевести текущий мип-уровень в eTransferSrcOptimal для следующей итерации
                    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                        .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                        .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                        .setDstAccessMask(vk::AccessFlagBits::eTransferRead);

                    cmd_buffer->pipelineBarrier(
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, 0, nullptr, 0, nullptr, 1, &barrier);

                    // Обновить текущий размер для следующей итерации
                    current_extent = next_extent;
                }

                // 4. Перевести все мип-уровни в eShaderReadOnlyOptimal для использования в шейдерах
                barrier.setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(aspect)
                        .setBaseMipLevel(0)
                        .setLevelCount(mip_levels_)
                        .setBaseArrayLayer(0)
                        .setLayerCount(layer_count))
                    .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
                    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

                cmd_buffer->pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    {}, 0, nullptr, 0, nullptr, 1, &barrier);

                // Завершить запись команд
                cmd_buffer->end();

                // Создать забор
                fence = vk_device_.createFenceUnique(vk::FenceCreateInfo());

                // Отправить команды в очередь
                queue.submit(vk::SubmitInfo().setCommandBuffers({cmd_buffer.get()}), fence.get());
            }

            // Ожидать завершения
            (void)vk_device_.waitForFences(
                {fence.get()},
                true,
                std::numeric_limits<uint64_t>::max());

            // Критическая секция (возможен многопоточный доступ к пулу)
            {
                std::lock_guard lock(queue_mutex);
                cmd_buffer.reset();
            }
        }

    private:
        /**
         * @brief Преобразует тип изображения в VkImageType
         * @param type Тип изображения
         * @return Соответствующий VkImageType
         */
        [[nodiscard]] static vk::ImageType vk_image_type(const Type& type) {
            switch(type){
                case Type::e1D:{
                    return vk::ImageType::e1D;
                }
                case Type::e2D:
                case Type::e2DArray:
                case Type::eCube:
                case Type::eCubeArray:{
                    return vk::ImageType::e2D;
                }
                case Type::e3D:{
                    return vk::ImageType::e3D;
                }
            }
            return vk::ImageType::e2D;
        }

        /**
         * @brief Преобразует тип изображения в VkImageViewType
         * @param type Тип изображения
         * @return Соответствующий VkImageViewType
         */
        [[nodiscard]] static vk::ImageViewType vk_image_view_type(const Type& type) {
            switch(type){
                case Type::e1D:{
                    return vk::ImageViewType::e1D;
                }
                case Type::e2D:{
                    return vk::ImageViewType::e2D;
                }
                case Type::e2DArray:{
                    return vk::ImageViewType::e2DArray;
                }
                case Type::eCube:{
                    return vk::ImageViewType::eCube;
                }
                case Type::eCubeArray:{
                    return vk::ImageViewType::eCubeArray;
                }
                case Type::e3D:{
                    return vk::ImageViewType::e3D;
                }
            }
            return vk::ImageViewType::e2D;
        }

    protected:
        /// Handle-объект логического устройства Vulkan
        vk::Device vk_device_;
        /// Handle внешнего изображения
        vk::Image image_;
        /// Представление изображения (для доступа/просмотра изображения)
        vk::UniqueImageView image_view_;
        /// Handle собственного изображения
        vk::UniqueImage own_image_;
        /// Память собственного изображения
        vk::UniqueDeviceMemory own_image_memory_;
        /// Количество мип-уровней
        uint32_t mip_levels_;
    };
}