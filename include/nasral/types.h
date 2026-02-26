#pragma once

#include <nasral/log/types.h>
#include <nasral/ecs/types.h>
#include <nasral/res/types.h>
#include <nasral/gfx/types.h>
#include <nasral/scn/types.h>
#include <nasral/inp/types.h>

namespace nasral
{
    struct Config
    {
        log::Config log = {};
        ecs::Config ecs = {};
        res::Config res = {};
        gfx::Config gfx = {};
        scn::Config scn = {};
        inp::Config inp = {};
    };
}