// #include "pch.h"
#include <nasral/scn/system.h>

namespace nasral::scn
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
