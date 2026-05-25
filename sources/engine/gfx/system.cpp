#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/gfx/renderer.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    System::System(Renderer* m) : ecs::System<System, Renderer>(m){}

    System::~System(){}

    void System::init()
    {
        log_info("Graphics ECS-system initialized");
    }

    void System::update([[maybe_unused]] const float delta)
    {
    }

    void System::finalize()
    {
        log_info("Graphics ECS-system finalized");
    }
}
