/**
 * @file buffer.hpp
 * @brief RAII обертка для управления буферами Vulkan
 * @author Alex "DarkWolf" Nem
 * @date 2025
 * @copyright MIT License
 *
 * Файл содержит реализацию класса Buffer, который инкапсулирует работу
 * с буферами Vulkan, включая их создание и управление памятью
 */

#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/utils/device.hpp>

namespace vk::utils
{
    /**
     * @class Buffer
     * @brief Обёртка над Vulkan-буфером, предоставляющая удобный интерфейс для работы с буферами.
     *
     * Класс предоставляет методы создания, управления и доступа к буферам на уровне Vulkan.
     * Упрощает работу с памятью, отображением (mapping) и другими операциями, связанными с буферами.
     */
    class Buffer
    {
    public:
        typedef std::unique_ptr<Buffer> Ptr;

        /**
         * @brief Конструктор по умолчанию.
         * Инициализирует объект класса с нулевым размером.
         */
        Buffer()
        : size_(0)
        {}

        /**
         * @brief Основной конструктор, создающий Vulkan-буфер.
         *
         * Инициализирует объект класса с указанными параметрами устройства, размером,
         * флагами использования и свойствами памяти.
         *
         * @param device Указатель на устройство Vulkan.
         * @param size Размер буфера в байтах.
         * @param usage Флаги, определяющие, как будет использоваться буфер (например, для хранения данных, индексов, вершин и т.п.).
         * @param properties Свойства памяти, которые должны быть настроены.
         * @param families Вектор с индексами очередей, которым должен быть предоставлен доступ к буферу.
         */
        Buffer(const Device::Ptr& device,
               const vk::DeviceSize size,
               const vk::BufferUsageFlags& usage,
               const vk::MemoryPropertyFlags& properties,
               const std::vector<uint32_t>& families = {})
        : vk_device_(device->logical_device())
        , size_(size)
        {
            assert(vk_device_);

            // Создание объекта буфера
            vk_buffer_ = vk_device_.createBufferUnique(
                vk::BufferCreateInfo()
                .setSize(size_)
                .setUsage(usage)
                .setSharingMode(families.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
                .setQueueFamilyIndices(families)
                .setFlags({}));

            // Получить индекс типа памяти
            const auto mem_reqs = vk_device_.getBufferMemoryRequirements(vk_buffer_.get());
            const auto mem_type_id = device->find_memory_type_index(mem_reqs, properties);
            if (!mem_type_id.has_value()){
                throw std::runtime_error("Failed to find suitable memory type");
            }

            // Флаг для возможности обращаться из shader'а к памяти устройства по адресу
            vk::MemoryAllocateFlagsInfoKHR allocate_flags{};
            allocate_flags.setFlags(vk::MemoryAllocateFlagBitsKHR::eDeviceAddress);

            // Выделить память буфера
            try
            {
                // Выделить память
                vk_memory_ = vk_device_.allocateMemoryUnique(
                    vk::MemoryAllocateInfo()
                    .setAllocationSize(size_)
                    .setMemoryTypeIndex(mem_type_id.value())
                    .setPNext(usage & vk::BufferUsageFlagBits::eShaderDeviceAddress ? &allocate_flags : nullptr));

                // Связать с объектом буфера
                vk_device_.bindBufferMemory(
                    vk_buffer_.get(),
                    vk_memory_.get(),
                    0);
            }
            catch(const ::vk::OutOfDeviceMemoryError& e) {
                vk_buffer_.reset();
                throw std::runtime_error("Failed to allocate buffer memory. " + std::string(e.what()));
            }
            catch(const ::vk::OutOfHostMemoryError& e) {
                vk_buffer_.reset();
                throw std::runtime_error("Failed to allocate buffer memory. " + std::string(e.what()));
            }
            catch(const std::exception& e) {
                vk_buffer_.reset();
                throw std::runtime_error("Failed to allocate buffer memory. " + std::string(e.what()));
            }
        }

        /** @brief Деструктор */
        ~Buffer() = default;

        /** @brief Запрет копирования */
        Buffer(const Buffer&) = delete;

        /** @brief Запрет присваивания */
        Buffer& operator=(const Buffer&) = delete;


        /**
         * @brief Отображает (maps) часть буфера в память хоста, чтобы можно было изменить данные.
         *
         * @param offset Смещение от начала буфера для отображения.
         * @param size Размер области, которую нужно отобразить. Если не указано, используется весь буфер.
         * @return Указатель на область памяти хоста.
         */
        void* map(const vk::DeviceSize offset = 0, const vk::DeviceSize size = VK_WHOLE_SIZE){
            assert(vk_device_);
            assert(vk_memory_);
            assert(size_ > 0 && (offset + size <= size_ || size == VK_WHOLE_SIZE));
            return vk_device_.mapMemory(vk_memory_.get(), offset, size, vk::MemoryMapFlags());
        }

        /**
         * @brief Снимает отображение (unmaps) части или всю область буфера.
         *
         * После вызова этой функции данные, изменённые через `map()`, будут записаны в память устройства.
         */
        void unmap(){
            assert(vk_device_);
            assert(vk_memory_);
            vk_device_.unmapMemory(vk_memory_.get());
        }

        /**
         * @brief Копирование данных в другой Vulkan буфер
         * @param other Другой Vulkan буфер
         * @param queue_group Группа очередей устройства с поддержкой команд копирования/перемещения
         */
        void copy_to(const Buffer& other, const Device::QueueGroup& queue_group){
            // Командный пул и очередь из группы с поддержкой команд копирования
            const auto& pool = queue_group.command_pools[0];
            const auto& queue = queue_group.queues[0];

            // Выделить командный буфер для исполнения команды копирования
            auto cmd_buffers = vk_device_.allocateCommandBuffersUnique(
                vk::CommandBufferAllocateInfo()
                .setCommandBufferCount(1)
                .setCommandPool(pool.get())
                .setLevel(vk::CommandBufferLevel::ePrimary));

            // Записать команду в буфер
            cmd_buffers[0].get().begin(
                vk::CommandBufferBeginInfo()
                .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

            cmd_buffers[0].get().copyBuffer(vk_buffer_.get(), other.vk_buffer_.get(), vk::BufferCopy()
                .setSrcOffset(0)
                .setDstOffset(0)
                .setSize(size_));

            cmd_buffers[0].get().end();

            // Отправить команду в очередь и подождать выполнения
            vk::SubmitInfo submit_info{};
            submit_info.setCommandBufferCount(1);
            submit_info.setPCommandBuffers(&cmd_buffers[0].get());
            queue.submit(submit_info, nullptr);
            queue.waitIdle();
        }

        /**
         * @brief Возвращает размер буфера.
         * @return Размер буфера в байтах.
         */
        [[nodiscard]] vk::DeviceSize size() const { return size_; }

        /**
         * @brief Возвращает объект Vulkan-буфера.
         * @return Объект-handle для Vulkan-буфера
         */
        [[nodiscard]] const vk::Buffer& vk_buffer() const { return vk_buffer_.get(); }

        /**
         * Возвращает объект памяти, связанной с буфером.
         * @return Объект-handle для Vulkan-памяти
         */
        [[nodiscard]] const vk::DeviceMemory& vk_memory() const { return vk_memory_.get(); }


    protected:
        /// Handle-объект логического устройства Vulkan
        vk::Device vk_device_;
        /// Handle-объект буфера Vulkan
        vk::UniqueBuffer vk_buffer_;
        /// Handle-объект памяти Vulkan
        vk::UniqueDeviceMemory vk_memory_;
        /// Размер буфера в байтах.
        vk::DeviceSize size_;
    };
}