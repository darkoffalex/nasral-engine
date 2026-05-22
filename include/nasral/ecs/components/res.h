#pragma once

#include <nasral/common/types.h>
#include <nasral/res/types.h>

namespace nasral::ecs
{
    struct ResourceComponent
    {
        res::ResourceId res_id;
    };

    template <typename E>
    struct ResourceListComponent
    {
        EnumArray<E, res::ResourceId> res_ids;
    };

    struct ResourceRequestComponent{};

    struct ResourceReleaseComponent{};

    struct ResourceErrorComponent{};

    struct ResourceLoadedComponent{};
}