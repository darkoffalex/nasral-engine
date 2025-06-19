#pragma once
#include <string>

namespace nasral::logging { class Logger; }

namespace nasral
{
    struct EngineContext
    {
        logging::Logger* logger = nullptr;
    };

    struct EngineConfig
    {
        struct LogConfig
        {
            std::string log_file;
            bool log_to_console = false;
        };

        LogConfig log_config;
    };
}