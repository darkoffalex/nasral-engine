#pragma once

namespace nasral
{
    class Engine;
}

namespace nasral::core
{
    /**
     * @brief Базовая специализация (с Config)
     * @tparam Config Тип конфигурации подсистемы
     */
    template<typename Config = void>
    class Subsystem
    {
    public:
        Subsystem(Engine* engine, const Config& config)
        : engine_(engine)
        , config_(config)
        {}

        [[nodiscard]] Engine* engine() const { return engine_; }
        [[nodiscard]] const Config& config() const { return config_; }

    private:
        Engine* const engine_;
        Config config_;
    };

    /**
     * @brief Специализация для варианта без Config
     */
    template<>
    class Subsystem<void>
    {
    public:
        explicit Subsystem(Engine* engine)
        : engine_(engine)
        {}

        [[nodiscard]] Engine* engine() const { return engine_; }

    private:
        Engine* const engine_;
    };
}