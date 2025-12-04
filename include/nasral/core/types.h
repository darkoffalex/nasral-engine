#pragma once

#include <magic_enum/magic_enum_containers.hpp>

namespace nasral::core
{
    template <typename E, typename V>
    using EnumArray = magic_enum::containers::array<E, V>;
}