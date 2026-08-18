/**
 * @file framebuffer.hpp
 * @brief RAII обертка для управления фрейм-буферами Vulkan
 * @author Alex "DarkWolf" Nem
 * @date 2025
 * @copyright MIT License
 *
 * Файл содержит реализацию класса Framebuffer, который инкапсулирует работу
 * с фрейм-буферами Vulkan и их вложениями (attachments). Предоставляет удобный
 * интерфейс для создания и управления фрейм-буферами с различными типами вложений.
 */

#pragma once
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/utils/image.hpp>

namespace vk::utils
{
    /**
     * @brief RAII обертка для управления фрейм-буферами Vulkan.
     *
     * Предоставляет высокоуровневый интерфейс для:
     *  создания фрейм-буферов с различными типами вложений,
     *  управления вложениями (цвет, глубина, трафарет),
     *  автоматического управления ресурсами вложений
     */
    class Framebuffer
    {
    public:
        typedef std::unique_ptr<Framebuffer> Ptr;

        /**
         * @brief Информация о вложении фрейм-буфера.
         *
         * Содержит параметры для создания или использования существующего вложения
         */
        struct AttachmentInfo
        {
            vk::Image image;              ///< Существующее изображение (nullptr для создания нового)
            vk::Format format;            ///< Формат вложения
            vk::ImageUsageFlags usage;    ///< Флаги использования изображения
            vk::ImageAspectFlags aspect;  ///< Аспекты вложения (цвет/глубина/трафарет)
            uint32_t mip_levels = 0;      ///< Кол-во потенциальных мип-уровней вложения (0 - автоматически)
        };

        /** @brief Конструктор по умолчанию */
        Framebuffer() = default;

        /**
         * @brief Создает фрейм-буфер с заданными параметрами
         *
         * @param device Устройство Vulkan (обертка из utils)
         * @param render_pass Проход рендеринга, с которым будет использоваться фрейм-буфер
         * @param extent Размеры фрейм-буфера
         * @param attachment_infos Информация о вложениях фрейм-буфера
         * @param queue_grough_indices Индексы групп очередей для shared/concurrent доступа
         * @throw std::runtime_error При ошибках создания фрейм-буфера или вложений
         *
         * @note Для каждого вложения автоматически создается Image и ImageView,
         * либо используется существующее изображение, если оно указано в AttachmentInfo
         */
        Framebuffer(Device* device,
                    const vk::RenderPass& render_pass,
                    const vk::Extent2D& extent,
                    const std::vector<AttachmentInfo>& attachment_infos,
                    const std::vector<size_t>& queue_grough_indices = {})
        : extent_(extent)
        {
            assert(device);
            assert(render_pass);
            assert(extent_.width > 0 && extent_.height > 0);
            assert(!attachment_infos.empty());

            // Получить индексы задействованных для работы с фрейм-буфером очередей
            std::vector<uint32_t> queue_family_indices = device->queue_family_indices(queue_grough_indices);

            // Создать вложения и подготовить массив image-view
            std::vector<vk::ImageView> views;
            for (const auto& info : attachment_infos)
            {
                Image::Ptr attachment = nullptr;
                UniqueImageView attachment_view{};

                // На базе существующего изображения
                if(info.image){
                    // Создаст только image-view для семплинга в шейдере
                    attachment = std::make_unique<Image>(
                            device,
                            Image::Type::e2D,
                            info.image,
                            info.format,
                            info.aspect);

                    // Создать image-vew вложения кадрового буфера
                    attachment_view = device->logical_device().createImageViewUnique(
                            ImageViewCreateInfo()
                            .setImage(info.image)
                            .setViewType(vk::ImageViewType::e2D)
                            .setFormat(info.format)
                            .setSubresourceRange(
                                    ImageSubresourceRange()
                                    .setAspectMask(info.aspect)
                                    .setBaseMipLevel(0)
                                    .setLevelCount(1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1)
                            )
                    );
                }
                // Создать новое изображение
                else{
                    // Выделяет память ресурса изображения + image-view для семплинга в шейдере
                    attachment = std::make_unique<Image>(
                            device,
                            Image::Type::e2D,
                            info.format,
                            vk::Extent3D{extent_.width, extent_.height, 1},
                            info.usage,
                            vk::ImageTiling::eOptimal,
                            info.aspect,
                            vk::MemoryPropertyFlagBits::eDeviceLocal,
                            vk::ImageLayout::eUndefined,
                            vk::SampleCountFlagBits::e1,
                            info.mip_levels,
                            1,
                            queue_family_indices);

                    // Создать image-vew вложения кадрового буфера
                    attachment_view = device->logical_device().createImageViewUnique(
                            ImageViewCreateInfo()
                            .setImage(attachment->image())
                            .setViewType(vk::ImageViewType::e2D)
                            .setFormat(info.format)
                            .setSubresourceRange(
                                    ImageSubresourceRange()
                                    .setAspectMask(info.aspect)
                                    .setBaseMipLevel(0)
                                    .setLevelCount(1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1)
                            )
                    );
                }

                // Добавить объекты в списки
                attachments_.emplace_back(std::move(attachment));
                attachment_views_.emplace_back(std::move(attachment_view));
            }

            // Сформировать список views для инициализации буфера
            for (auto& view : attachment_views_){
                views.push_back(view.get());
            }

            // Создать кадровый буфер
            framebuffer_ = device->logical_device().createFramebufferUnique(
                    vk::FramebufferCreateInfo()
                    .setRenderPass(render_pass)
                    .setAttachments(views)
                    .setWidth(extent_.width)
                    .setHeight(extent_.height)
                    .setLayers(1));
        }

        /** @brief Деструктор */
        ~Framebuffer() = default;

        /** @brief Запрет копирования */
        Framebuffer(const Framebuffer&) = delete;

        /** @brief Запрет присваивания */
        Framebuffer& operator=(const Framebuffer&) = delete;

        /**
         * @brief Возвращает размеры фрейм-буфера
         * @return Константная ссылка на размеры фрейм-буфера
         */
        [[nodiscard]] const vk::Extent2D& extent() const {
            return extent_;
        }

        /**
         * @brief Возвращает handle фрейм-буфера
         * @return Константная ссылка на фрейм-буфер Vulkan
         */
        [[nodiscard]] const vk::Framebuffer& vk_framebuffer() const {
            return framebuffer_.get();
        }

        /**
         * @brief Возвращает список вложений фрейм-буфера
         * @return Константная ссылка на вектор умных указателей на вложения
         */
        [[nodiscard]] const std::vector<Image::Ptr>& attachments() const {
            return attachments_;
        }

    protected:
        vk::Extent2D extent_;                           ///< Размеры фрейм-буфера
        vk::UniqueFramebuffer framebuffer_;             ///< Handle фрейм-буфера
        std::vector<Image::Ptr> attachments_;           ///< Список вложений
        std::vector<UniqueImageView> attachment_views_; ///< Список views вложений

    };
}