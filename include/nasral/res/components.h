#pragma once

#include <nasral/common/types.h>
#include <nasral/res/types.h>

namespace nasral::res
{
    struct IdComponent
    {
        ResourceId res_id;
    };

    template <typename E>
    struct IdListComponent
    {
        EnumArray<E, ResourceId> res_ids;
    };

    struct RequestComponent{};

    struct ReleaseComponent{};

    struct ErrorComponent{};

    struct LoadedComponent{};
}
