#pragma once

#include <vulkan/vulkan.hpp>

namespace nasral::gfx
{
    inline vk::DeviceSize size_align(const vk::DeviceSize size, const vk::DeviceSize alignment){
        return (size + alignment - 1) & ~(alignment - 1);
    }

    template<typename T>
    vk::DeviceSize aligned_ubo(const vk::PhysicalDevice& device){
        const auto align = device.getProperties().limits.minUniformBufferOffsetAlignment;
        return size_align(sizeof(T), align);
    }

    template<typename T>
    vk::DeviceSize aligned_sbo(const vk::PhysicalDevice& device){
        const auto align = device.getProperties().limits.minStorageBufferOffsetAlignment;
        return size_align(sizeof(T), align);
    }

    template<typename T>
    vk::DeviceSize ubo_offset(const vk::PhysicalDevice& device, const uint32_t index){
        return aligned_ubo<T>(device) * index;
    }

    template<typename T>
    vk::DeviceSize sbo_offset(const vk::PhysicalDevice& device, const uint32_t index){
        return aligned_sbo<T>(device) * index;
    }
}