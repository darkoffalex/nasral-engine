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

    template <typename T>
    std::optional<T> from_arg(const Arg& arg) {
        if constexpr (std::is_pointer_v<T>){
            if (auto* vptr = std::get_if<void*>(&arg)){
                return {static_cast<T>(*vptr)};
            }
        }else{
            if (auto* ptr = std::get_if<T>(&arg)){
                return {*ptr};
            }
        }
        return std::nullopt;
    }
}