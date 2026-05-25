#pragma once

#include <nasral/common/types.h>

namespace nasral::ecs
{
    struct ActivateComponent{};

    struct DeactivateComponent{};

    struct DestroyComponent{};

    struct UidComponent
    {
        UniqueId id = {};
    };

    struct NameComponent
    {
        std::string name = {};
    };
}
