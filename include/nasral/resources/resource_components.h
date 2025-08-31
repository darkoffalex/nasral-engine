#pragma once
#include <nasral/resources/resource_types.h>
#include <nasral/resources/request.h>

namespace nasral::resources::components
{
    struct Resource
    {
        Type type;
        std::string_view path;
    };

    struct ResourceHandle
    {
        Type type;
        Request request;
    };
}