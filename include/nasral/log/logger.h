#pragma once

#include <memory>
#include <fstream>
#include <mutex>

#include <nasral/common/subsystem.h>
#include <nasral/log/types.h>

namespace nasral::log
{
    class Logger final : public Subsystem<Logger, Config>
    {
    public:
        typedef std::unique_ptr<Logger> Ptr;

        Logger(Engine* engine, const Config& config);
        ~Logger();

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void log_unsafe(Level level, const std::string& message);
        void log(Level level, const std::string& message);

        void debug(const std::string& message){
            log(Level::eDebug, message);
        }

        void info(const std::string& message){
            log(Level::eInfo, message);
        }

        void warn(const std::string& message){
            log(Level::eWarning, message);
        }

        void error(const std::string& message){
            log(Level::eError, message);
        }

        void fatal(const std::string& message){
            log(Level::eFatal, message);
        }

    private:
        std::ofstream fs_;
        std::mutex mutex_;
    };
}