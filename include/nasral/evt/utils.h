#pragma once

#include <nasral/evt/types.h>

namespace nasral::evt
{
    template <typename Class, typename Method>
    auto bind(Class* obj, Method method) {
        return [obj, method](const Arg& arg) {
            (obj->*method)(arg);
        };
    }
}