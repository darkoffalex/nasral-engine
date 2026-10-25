#include "pch.h"
#include <nasral/gfx/renderer.h>

namespace nasral::gfx
{
    VkBool32 Renderer::vk_debug_report_callback(
        [[maybe_unused]] vk::Flags<vk::DebugReportFlagBitsEXT> flags,
        [[maybe_unused]] vk::DebugReportObjectTypeEXT object_type,
        [[maybe_unused]] uint64_t obj,
        [[maybe_unused]] size_t location,
        [[maybe_unused]] int32_t code,
        const char* layer_prefix,
        const char* msg,
        void* user_data)
    {
        auto* logger = static_cast<log::Logger*>(user_data);
        const std::string msg_str = "Vulkan validation: " + std::string(layer_prefix) + " | " + std::string(msg) + "\n";
        logger->warn(msg_str);
        return VK_FALSE;
    }

    Renderer::Renderer(Engine* e, const Config& config)
        : Subsystem(e, config)
        , is_active_(false)
        , is_frame_started_(false)
        , is_surface_dirty_(false)
        , frame_count_(0)
        , frame_index_(0)
        , available_swapchain_image_(0)
        , vk_last_rasterization_pipeline_(VK_NULL_HANDLE)
    {

    }

    Renderer::~Renderer()
    {
    }
}
