#include "pch.h"
#include <nasral/logging/logger.h>

namespace nasral::logging
{
    Logger::Logger(const LoggingConfig& config)
        : console_out_(config.console_out)
    {
        try {
            if (!config.file.empty()){
                fs_.open(config.file, std::ios::out | std::ios::app);
            }
        }catch(const std::exception& e) {
            throw LoggerError(e.what());
        }
    }

    Logger::~Logger(){
        if (fs_.is_open()){
            fs_.close();
        }
    }

    void Logger::log(const Level level, const std::string& message) const noexcept{
        std::lock_guard lock(mutex_);
        log_unsafe(level, message);
    }

    void Logger::log_unsafe(const Level level, const std::string& message) const noexcept{
        std::string level_str;
        switch (level){
            case Level::eDebug: level_str = "DEBUG"; break;
            case Level::eInfo: level_str = "INFO"; break;
            case Level::eWarning: level_str = "WARNING"; break;
            case Level::eError: level_str = "ERROR"; break;
        }

        const std::string log_str = "[" + level_str + "] " + message + "\n";

        if (console_out_){
            if (level == Level::eError){
                std::cerr << log_str;
                std::flush(std::cerr);
            }else{
                std::cout << log_str;
                std::flush(std::cout);
            }
        }

        if (fs_.is_open()){
            fs_ << log_str;
            fs_.flush();
        }
    }

    void Logger::debug(const std::string& message) const noexcept{
        log(Level::eDebug, message);
    }

    void Logger::info(const std::string& message) const noexcept{
        log(Level::eInfo, message);
    }

    void Logger::warning(const std::string& message) const noexcept{
        log(Level::eWarning, message);
    }

    void Logger::error(const std::string& message) const noexcept{
        log(Level::eError, message);
    }
}
