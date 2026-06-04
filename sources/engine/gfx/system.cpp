#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/gfx/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/view.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    System::System(Manager* m) : ecs::System<System, Manager>(m){}
    System::~System() = default;

    void System::on_init()
    {
        log_info("ECS-system initialized");
    }

    void System::on_update([[maybe_unused]] const float delta)
    {
    }

    void System::on_finalize()
    {
        log_info("ECS-system finalized");
    }

    void System::process_materials_update()
    {
    }

    void System::process_objects_update()
    {
    }
}
