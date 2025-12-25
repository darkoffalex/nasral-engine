// #include "pch.h"
#include <nasral/scn/manager.h>

namespace nasral::scn
{
    Manager::Manager(Engine* e, const Config& cfg)
        : Subsystem(e, cfg)
        , root_({})
    {
    }

    Manager::~Manager()
    = default;
}
