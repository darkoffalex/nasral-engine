#pragma once

#include <memory>
#include <fstream>
#include <mutex>

#include <nasral/core/subsystem.h>
#include <nasral/log/types.h>

namespace nasral::log
{
    class Logger final : public core::Subsystem<Config>
    {
    public:
        typedef std::unique_ptr<Logger> Ptr;

        Logger(Engine* engine, const Config& config);
        ~Logger();

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void log_unsafe(Level level, const std::string& message);
        void log(Level level, const std::string& message);
        void debug(const std::string& message);
        void info(const std::string& message);
        void warn(const std::string& message);
        void error(const std::string& message);
        void fatal(const std::string& message);

    private:
        std::ofstream fs_;
        std::mutex mutex_;
    };
}