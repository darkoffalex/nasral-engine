/**
 * @file acceleration_structure.hpp
 * @brief RAII обертка для управления структурой ускорения для Ray Tracing
 * @author Alex "DarkWolf" Nem
 * @date 2026
 * @copyright MIT License
 *
 * Файл содержит реализацию класса AccelerationStructure, который инкапсулирует работу
 * с буферами Vulkan, включая их создание и управление памятью
 */

#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/utils/buffer.hpp>

namespace vk::utils
{
    class AccelerationStructure
    {
    public:
        using Ptr = std::unique_ptr<AccelerationStructure>;
        using UniqueAS = vk::UniqueHandle<vk::AccelerationStructureKHR, vk::detail::DispatchLoaderDynamic>;

        AccelerationStructure(Device* device,
            const vk::AccelerationStructureTypeKHR type,
            const vk::DeviceSize size,
            const vk::detail::DispatchLoaderDynamic& loader) : vk_device_(device->logical_device())
        {
            // Буфер для хранения структуры ускорения
            buffer_ = std::make_unique<Buffer>(
                device,
                size,
                vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );

            // Создание объекта AccelerationStructure
            vk_as_ = vk_device_.createAccelerationStructureKHRUnique(
                vk::AccelerationStructureCreateInfoKHR()
                    .setBuffer(buffer_->vk_buffer())
                    .setOffset(0)
                    .setSize(size)
                    .setType(type),
                    nullptr,
                    loader
            );

            // Получение адреса структуры для TLAS instances
            device_address_ = vk_device_.getAccelerationStructureAddressKHR(
                vk::AccelerationStructureDeviceAddressInfoKHR().setAccelerationStructure(vk_as_.get()), loader
            );
        }

        [[nodiscard]] const vk::AccelerationStructureKHR& handle() const noexcept { return vk_as_.get(); }
        [[nodiscard]] vk::DeviceAddress device_address() const noexcept { return device_address_; }
        [[nodiscard]] const Buffer& buffer() const noexcept { return *buffer_; }

    private:
        vk::Device vk_device_;
        Buffer::Ptr buffer_;
        UniqueAS vk_as_;
        vk::DeviceAddress device_address_ = 0;
    };
}