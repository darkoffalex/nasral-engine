#include "pch.h"
#include <nasral/rendering/renderer.h>
#include <nasral/engine.h>

namespace nasral::rendering
{
    Renderer::Renderer(const Engine *engine, RenderingConfig config)
        : engine_(engine)
        , config_(std::move(config))
        , is_rendering_(false)
        , surface_refresh_required_(false)
        , current_frame_(0)
        , available_image_index_(0)
    {

    }

    Renderer::~Renderer() = default;

    const logging::Logger* Renderer::logger() const{
        return engine()->logger();
    }
}