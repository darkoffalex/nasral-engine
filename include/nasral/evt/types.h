#pragma once

#include <variant>
#include <string>
#include <nasral/core/types.h>

namespace nasral::evt
{
    enum class Type : uint32_t
    {
        eProjectResLoaded = 0,
        eProjectResReleasing,
        TOTAL
    };

    using Arg = std::variant<
        void*,
        uint32_t,
        int32_t,
        float,
        double,
        std::string,
        std::string_view,
        core::UniqueId
    >;

    using Listener = std::function<void(const Arg&)>;
    using ListenerHandle = size_t;

    constexpr ListenerHandle    kInvalidListener = static_cast<ListenerHandle>(-1);
    constexpr size_t            kInitialListenersCount = 32;
}