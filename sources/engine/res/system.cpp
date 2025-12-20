#include "pch.h"
#include <nasral/res/system.h>
#include <nasral/ecs/manager.h>

namespace nasral::res
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
