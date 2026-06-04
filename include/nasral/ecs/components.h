#pragma once

#include <nasral/common/types.h>
#include <nasral/ecs/entity.h>
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

    struct EntityComponent
    {
        EntityId id = {};
    };

    struct EntityListComponent
    {
        EntityIds<> ids = {};
    };
}
