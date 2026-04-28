#pragma once

#include <string>
#include <nasral/log/logger.h>

namespace nasral::log
{
    /**
     * @brief Пустая специализация класса, обеспечивающая доступ к логгеру
     * @tparam T Тип объекта, предоставляющего указатель на логгер
     * @tparam Enable Специализация по умолчанию (без доступа - exception)
     */
    template<typename T, typename Enable = void>
    struct LoggerAccessor{
        static Logger* get([[maybe_unused]] const T* obj){
            static_assert(sizeof(T) == 0, "LoggerAccessor not specialized for this type");
            return nullptr;
        }
    };

    /**
     * @brief Базовая специализация CRTP-класса loggable объекта
     * @details Наследники класса смогут использовать log_ методы напрямую
     * @tparam Derived Тип класса наследника
     */
    template<typename Derived>
    class Loggable
    {
    public:
        void log_debug(const std::string& message) const{
            auto* logger = LoggerAccessor<Derived>::get(static_cast<const Derived*>(this));
            logger->debug(message);
        }

        void log_info(const std::string& message) const{
            auto* logger = LoggerAccessor<Derived>::get(static_cast<const Derived*>(this));
            logger->info(message);
        }

        void log_warn(const std::string& message) const{
            auto* logger = LoggerAccessor<Derived>::get(static_cast<const Derived*>(this));
            logger->warn(message);
        }

        void log_error(const std::string& message) const{
            auto* logger = LoggerAccessor<Derived>::get(static_cast<const Derived*>(this));
            logger->error(message);
        }

        void log_fatal(const std::string& message) const{
            auto* logger = LoggerAccessor<Derived>::get(static_cast<const Derived*>(this));
            logger->fatal(message);
        }
    };
}

/**
 * @brief Макрос, объявляющий специализацию LoggerAccessor для конкретной подсистемы
 * @param Type Класс конкретной подсистемы (Subsystem)
 */
#define DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(Type) \
namespace nasral::log \
{ \
    class Logger; \
    template <typename T> \
    struct LoggerAccessor<T, std::enable_if_t<std::is_same_v<Type, T>>> { \
        static Logger* get(const T* mgr) { \
            return mgr->engine()->logger(); \
        } \
    }; \
}

/**
 * @brief Макрос, объявляющий специализацию LoggerAccessor для объекта подсистемы
 * @param Type Класс конкретного объекта подсистемы (SubsystemObject)
 */
#define DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(Type) \
namespace nasral::log \
{ \
    class Logger; \
    template <typename T> \
    struct LoggerAccessor<T, std::enable_if_t<std::is_same_v<Type, T>>> { \
        static Logger* get(const T* obj) { \
            return obj->subsystem()->engine()->logger(); \
        } \
    }; \
}
