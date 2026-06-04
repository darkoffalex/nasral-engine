#pragma once

#include <nasral/res/types.h>

namespace nasral::res
{
    struct ResourcesComponent
    {
        typedef std::array<ResourceId, kResListComponentSize> IdsList;
        typedef std::array<bool, kResListComponentSize> ActiveList;
        typedef std::array<Status, kResListComponentSize> StatusList;

        IdsList ids = {kInvalidResourceId};
        ActiveList active = {false};
        StatusList statuses = {Status::eUnloaded};
    };

    struct RequestComponent{};

    struct LoadingComponent{};

    struct LoadedComponent{};

    struct ReleaseComponent{};

    struct ErrorComponent{};
}