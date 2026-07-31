#pragma once

#include <nasral/common/types.h>
#include <nasral/ecs/entity.h>

namespace nasral::ecs
{
    struct ActivateComponent{};

    struct DeactivateComponent{};

    struct DestroyComponent{};

    struct RefsChangedComponent {};

    struct UidComponent
    {
        UniqueId id = {};
    };

    struct NameComponent
    {
        std::string name = {};
    };

    struct EntityComponent
    {
        EntityId id = {};
    };

    struct EntityListComponent
    {
        EntityIds<> ids = {};
    };

    struct RefsCountComponent
    {
        uint32_t count = 0;
    };
}
