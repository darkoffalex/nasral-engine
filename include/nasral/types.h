#pragma once

#include <nasral/log/types.h>
#include <nasral/ecs/types.h>
#include <nasral/res/types.h>
#include <nasral/gfx/types.h>

namespace nasral
{
    struct Config
    {
        log::Config log = {};
        ecs::Config ecs = {};
        res::Config res = {};
        gfx::Config gfx = {};
    };
}