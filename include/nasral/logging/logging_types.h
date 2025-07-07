#pragma once
#include <string>

namespace nasral::logging
{
    struct LoggingConfig
    {
        std::string file;
        bool console_out = false;
    };
}