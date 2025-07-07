#include "pch.h"
#include <nasral/rendering/renderer.h>
#include <nasral/engine.h>

namespace nasral::rendering
{
    Renderer::Renderer(nasral::Engine *engine, const RenderingConfig &config)
        : engine_(engine)
    {

    }

    Renderer::~Renderer() = default;

    const logging::Logger* Renderer::logger() const{
        return engine()->logger();
    }
}