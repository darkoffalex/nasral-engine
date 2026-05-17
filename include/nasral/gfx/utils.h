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

    inline vk::Format get_image_vk_format(const uint32_t channel_count, const uint32_t channel_depth, const bool srgb)
    {
        // 1 байт (8 бит) на канал
        if (channel_depth == 1) {
            switch (channel_count) {
            case 1: return srgb ? vk::Format::eR8Srgb : vk::Format::eR8Unorm;
            case 2: return srgb ? vk::Format::eR8G8Srgb : vk::Format::eR8G8Unorm;
            case 3: return srgb ? vk::Format::eR8G8B8Srgb : vk::Format::eR8G8B8Unorm;
            case 4: return srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
            default: return vk::Format::eUndefined;
            }
        }
        // 2 байта (16 бит) на канал
        if (channel_depth == 2) { // 16-bit per channel
            switch (channel_count) {
            case 1: return vk::Format::eR16Unorm;
            case 2: return vk::Format::eR16G16Unorm;
            case 3: return vk::Format::eR16G16B16Unorm;
            case 4: return vk::Format::eR16G16B16A16Unorm;
            default: return vk::Format::eUndefined;
            }
        }
        // 4 байта (32 бит) на канал (HDR текстуры)
        if (channel_depth == 4) {
            switch (channel_count) {
            case 1: return vk::Format::eR32Sfloat;
            case 2: return vk::Format::eR32G32Sfloat;
            case 3: return vk::Format::eR32G32B32Sfloat;
            case 4: return vk::Format::eR32G32B32A32Sfloat;
            default: return vk::Format::eUndefined;
            }
        }
        return vk::Format::eUndefined;
    }
}