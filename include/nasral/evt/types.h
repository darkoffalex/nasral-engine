#pragma once

#include <variant>
#include <string>
#include <nasral/common/types.h>

namespace nasral::evt
{
    /**
     * @brief Типы событий
     */
    enum class Type : uint32_t
    {
        // Файл проекта загружен (доступен для чтения)
        eProjectFileLoaded = 0,
        // Файл проекта обновлен (внесены изменения в документ)
        eProjectFileUpdated,
        // Регистр ресурсов обновлен (в RAM)
        eResourceRegistryChanged,
        // Регистр материалов обновлен (в RAM)
        eMaterialRegistryChanged,
        // Сеанс движка запущен
        eSessionStarted,
        // Основная сцена загружена
        eRootSceneLoaded,
        // Событие пользователя (пользовательская логика)
        eUserEvent,
        TOTAL
    };

    /**
     * @brief Типы изменений (для Changed событий)
     */
    enum class ChangeReason : uint32_t
    {
        eInitial = 0,
        eAdded,
        eRemoved,
        eUpdated,
        eRebuilt,
        TOTAL
    };

    /**
     * @brief Аргумент событий (варианты)
     */
    using Arg = std::variant<
        void*,
        uint32_t,
        int32_t,
        uint64_t,
        int64_t,
        float,
        double,
        std::string,
        std::string_view,
        UniqueId,
        ChangeReason
    >;

    using ListenerCallback = std::function<void(const Arg&)>;
    using ListenerHandle = size_t;

    constexpr ListenerHandle    kInvalidListener = static_cast<ListenerHandle>(-1);
    constexpr size_t            kInitialListenersCount = 32;
    constexpr uint32_t          kNullArg = 0;
}