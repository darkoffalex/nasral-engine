#pragma once
#include <type_traits>

namespace nasral::core
{
    template<typename Derived>
    struct Component
    {
        Component()
        {
            static_assert(std::is_default_constructible_v<Derived>, "All ECS components MUST be default-constructible.");
        }
    };
}
