#pragma once
#include <nasral/logging/logging_types.h>

namespace nasral::logging
{
    class Logger
    {
    public:
        typedef std::unique_ptr<Logger> Ptr;
        enum class Level : unsigned
        {
            eDebug = 0,
            eInfo,
            eWarning,
            eError
        };

        explicit Logger(const LoggingConfig& config);
        ~Logger();

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void log_unsafe(Level level, const std::string& message) const;
        void log(Level level, const std::string& message) const;
        void debug(const std::string& message) const;
        void info(const std::string& message) const;
        void warning(const std::string& message) const;
        void error(const std::string& message) const;

    private:
        mutable std::ofstream fs_;
        mutable std::mutex mutex_;
        bool console_out_;
    };
}

