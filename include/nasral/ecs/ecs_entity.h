#pragma once
#include <nasral/core_types.h>

namespace nasral::ecs
{
    struct EntityId
    {
        size_t index = 0;
        size_t version = 0;
        bool operator==(const EntityId& other) const{
            return index == other.index && version == other.version;
        }
    };
}