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

    using ArgPtr = void*;
    union ArgNumeric { uint32_t u; int32_t i; float f; double d; };
    using ArgString = std::string;
    using ArgStringView = std::string_view;
    using ArgUniqueId = core::UniqueId;

    using Arg = std::variant<
        ArgPtr,
        ArgNumeric,
        ArgString,
        ArgStringView,
        ArgUniqueId
    >;

    using Listener = std::function<void(const Arg&)>;
    using ListenerHandle = size_t;

    constexpr ListenerHandle    kInvalidListener = static_cast<ListenerHandle>(-1);
    constexpr size_t            kInitialListenersCount = 32;
}