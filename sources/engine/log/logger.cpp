#include "pch.h"
#include <nasral/log/logger.h>

namespace nasral::log
{
    Logger::Logger(Engine* engine, const Config& config)
    : Subsystem(engine, config)
    {
        if (!config.file.empty()){
            fs_.open(config.file, std::ios::out | std::ios::app);

            if (!fs_.is_open() || fs_.fail()){
                throw std::runtime_error("Failed to open log file");
            }
        }
    }

    Logger::~Logger(){
        if (fs_.is_open()){
            fs_.close();
        }
    }

    void Logger::log_unsafe(Level level, const std::string& message)
    {
        // Если требуемый уровень логирования отключен - выйти
        if ((config().level & level) != level){
            return;
        }

        // Биты уровней
        const std::bitset<sizeof(Level)> bits(static_cast<uint32_t>(level));

        // Итоговое сообщение (уровни + сообщение)
        std::string result;
        for (size_t i = 0; i < bits.size(); ++i){
            if (bits.test(i)){
                result += kLevelNames[i];
            }
        }
        result += " " + message + "\n";

        // Вывод в консоль если нужно
        if (config().console){
            if (level == Level::eError || level == Level::eFatal){
                std::cerr << result;
                std::flush(std::cerr);
            }else{
                std::cout << result;
                std::flush(std::cout);
            }
        }

        // Вывод в файл если указан
        if (fs_.is_open()){
            fs_ << result;
            fs_.flush();
        }
    }

    void Logger::log(const Level level, const std::string& message){
        std::lock_guard lock(mutex_);
        log_unsafe(level, message);
    }
}
