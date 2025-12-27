#pragma once
#include <cstddef>

namespace nasral::ecs
{
    struct EntityId
    {
        size_t index;
        size_t version;

        bool operator==(const EntityId& other) const{
            return index == other.index && version == other.version;
        }
    };
}
