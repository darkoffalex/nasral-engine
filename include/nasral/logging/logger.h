#pragma once

namespace nasral::logging
{
    class Logger
    {
    public:
        enum class Level : unsigned
        {
            eDebug = 0,
            eInfo,
            eWarning,
            eError
        };

        explicit Logger(const std::string& log_file = "engine.log", bool console_out = true);
        ~Logger();

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void log(Level level, const std::string& message);
        void log_unsafe(Level level, const std::string& message);
        void debug(const std::string& message);
        void info(const std::string& message);
        void warning(const std::string& message);
        void error(const std::string& message);

    private:
        std::ofstream fs_;
        std::mutex mutex_;
        bool console_out_;
    };
}

#define LOG(context, level, msg) if(context.logger) context.logger->log(level, msg);
#define LOG_DEBUG(context, msg) if(context.logger) context.logger->debug(msg);
#define LOG_INFO(context, msg) if(context.logger) context.logger->info(msg);
#define LOG_WARNING(context, msg) if(context.logger) context.logger->warning(msg);
#define LOG_ERROR(context, msg) if(context.logger) context.logger->error(msg);

