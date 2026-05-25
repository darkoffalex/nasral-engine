#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/gfx/manager.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    System::System(Manager* m) : ecs::System<System, Manager>(m){}

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
