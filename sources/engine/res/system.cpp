#include "pch.h"
#include <nasral/res/system.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    System::System(Manager* m) : ecs::System<System, Manager>(m){}

    System::~System(){}

    void System::init()
    {
        log_info("Resource ECS-system initialized");
    }

    void System::update([[maybe_unused]] const float delta)
    {
    }

    void System::finalize()
    {
        log_info("Resource ECS-system finalized");
    }
}
