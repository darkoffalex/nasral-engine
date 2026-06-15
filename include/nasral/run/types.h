#pragma once

#include <cstdint>

namespace nasral::run
{
    /**
     * @brief Состояние среды выполнения
     */
    enum class StateFlags : uint32_t
    {
        eRunning = 0,
        ePaused,
        eResourcesReady,
        eMaterialsReady,
        TOTAL
    };

    /**
     * @brief Конфигурация подсистемы
     */
    struct Config
    {
        float target_fps = 60.0f;
    };
}
