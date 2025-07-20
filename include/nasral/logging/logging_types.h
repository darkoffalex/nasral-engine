#pragma once
#include <string>
#include <nasral/core_types.h>

namespace nasral::logging
{
    struct LoggingConfig
    {
        std::string file;
        bool console_out = false;
    };

    class LoggerError final : public EngineError
    {
    public:
        explicit LoggerError(const std::string& message)
        : EngineError(message) {}
    };
}