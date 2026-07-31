#pragma once

#include <nasral/gfx/types.h>
#include <vulkan/vulkan.hpp>

namespace nasral::res { class Manager; }

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

    vk::Format get_image_vk_format(uint32_t channel_count, uint32_t channel_depth, bool srgb);

    GeometryData gen_quad_geometry(float size = 1.0f);

    GeometryData gen_cube_geometry(float size = 1.0f);

    GeometryData gen_sphere_geometry(
        float radius = 0.5f,
        uint32_t segments = 32,
        uint32_t rings = 16,
        bool clockwise = true,
        const glm::vec4& color = glm::vec4(1.0f));

    handles::Texture fallback_tex_h(const res::Manager* res_m, TextureType type);
}
