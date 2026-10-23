#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>

#include <vulkan/utils/device.hpp>
#include <vulkan/utils/uniform_layout.hpp>
#include <vulkan/utils/buffer.hpp>

namespace nasral::gfx
{
    class Manager;
    class Renderer final : public Subsystem<Renderer, Config>, public log::Loggable<Renderer>
    {
    private:
        // Состояние
        bool is_active_;
        bool is_frame_started_;
        std::atomic<bool> is_surface_dirty_;

        // Основные сущности Vulkan (включая кастомные RAII обертки)
        vk::UniqueInstance vk_instance_;
        VkDebugReportCallback vk_debug_callback_;
        vk::detail::DispatchLoaderDynamic vk_loader_;
        vk::UniqueSurfaceKHR vk_surface_;
        vk::utils::Device::Ptr vk_device_;
        vk::UniqueSwapchainKHR vk_swap_chain_;

        // TODO: Заимплементить обертку над новыми кадровыми буферами
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::Renderer, "GFX")