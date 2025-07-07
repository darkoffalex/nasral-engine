/**
 * @file image.hpp
 * @brief RAII обертка для управления изображениями Vulkan
 * @author Alex "DarkWolf" Nem
 * @date 2025
 * @copyright MIT License
 *
 * Этот файл содержит реализацию класса Image, который инкапсулирует работу
 * с изображениями Vulkan, включая их создание, управление памятью и представлениями.
 */

#pragma once
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/utils/device.hpp>

namespace vk::utils
{
    /**
     * @brief RAII обертка для управления изображениями Vulkan
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
         * @brief Тип изображения
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
         * @throw std::runtime_error при ошибках создания или выделения памяти
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
              : Image()
        {
            assert(device);
            assert(mip_levels > 0);
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
            auto sharing_mode = queue_family_indices.size() > 1 ?
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
            const auto mem_type_index = device->find_memory_type_index(mem_reqs.memoryTypeBits, memory_properties);
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

    protected:
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
        }

    private:
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