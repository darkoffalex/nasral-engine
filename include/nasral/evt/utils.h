#pragma once

#include <nasral/evt/types.h>

namespace nasral::evt
{
    /**
     * @brief Генерация лямбда выражения с обращением к методу объекта
     * @tparam Class Класс объекта
     * @tparam Method Имя метода объекта
     * @param obj Указатель на объект
     * @param method Метод
     * @return Лямбда выражение
     */
    template <typename Class, typename Method>
    auto bind(Class* obj, Method method) {
        return [obj, method](const Arg& arg) {
            (obj->*method)(arg);
        };
    }

    /**
     * @brief Извлечение параметра нужного типа из варианта аргумента
     * @tparam T Желаемый тип
     * @param arg Вариант аргумента
     * @return Optional значение (std::nullopt если нет подходящего типа)
     */
    template <typename T>
    std::optional<T> from_arg(const Arg& arg) {
        if constexpr (std::is_pointer_v<T>){
            if (auto* vptr = std::get_if<void*>(&arg)){
                return {static_cast<T>(*vptr)};
            }
        }else{
            if (auto* ptr = std::get_if<T>(&arg)){
                return {*ptr};
            }
        }
        return std::nullopt;
    }
}