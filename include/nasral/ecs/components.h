#pragma once

#include <nasral/common/types.h>
#include <nasral/ecs/entity.h>

namespace nasral::ecs
{
    struct ActivateComponent{};

    struct DeactivateComponent{};

    struct DestroyComponent{};

    struct RefsChangedComponent {};

    struct DirtyStateComponent{};

    struct UidComponent
    {
        UniqueId id = {};
    };

    struct NextComponent
    {
        UniqueId uid = {};
        EntityId entity = EntityId::invalid();
    };

    struct PrevComponent
    {
        UniqueId uid = {};
        EntityId entity = EntityId::invalid();
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
