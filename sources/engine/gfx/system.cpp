#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/ecs/manager.h>

namespace nasral::gfx
{
    System::System(Engine* engine) : ecs::System<System>(engine)
    {}

    System::~System()
    = default;

    void System::init()
    {}

    void System::update([[maybe_unused]] const float dt)
    {}

    void System::shutdown()
    {}
}
