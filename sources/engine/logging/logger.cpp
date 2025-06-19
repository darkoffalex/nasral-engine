#include "pch.h"
#include <nasral/logging/logger.h>

namespace nasral::logging
{
    Logger::Logger(const std::string& log_file, const bool console_out)
        : console_out_(console_out)
    {
        if (!log_file.empty()){
            fs_.open(log_file, std::ios::out | std::ios::app);
        }
    }

    Logger::~Logger(){
        if (fs_.is_open()){
            fs_.close();
        }
    }

    void Logger::log(const Level level, const std::string& message){
        std::lock_guard<std::mutex> lock(mutex_);
        log_unsafe(level, message);
    }

    void Logger::log_unsafe(const Level level, const std::string& message){
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
            }else{
                std::cout << log_str;
            }
        }

        if (fs_.is_open()){
            fs_ << log_str;
            fs_.flush();
        }
    }

    void Logger::debug(const std::string& message){
        log(Level::eDebug, message);
    }

    void Logger::info(const std::string& message){
        log(Level::eInfo, message);
    }

    void Logger::warning(const std::string& message){
        log(Level::eWarning, message);
    }

    void Logger::error(const std::string& message){
        log(Level::eError, message);
    }
}
