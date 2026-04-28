#pragma once
#include "types.h"

namespace nasral
{
    /**
     * @brief Forward declaration
     */
    class Engine;

    /**
     * @brief Базовая специализация CRTP-класса подсистемы
     * @tparam Derived Тип класса наследника
     * @tparam Config Тип структуры конфигурации
     */
    template<typename Derived, typename Config = void>
    class Subsystem
    {
    public:
        Subsystem(Engine* engine, const Config& config)
        : engine_(engine)
        , config_(config)
        {}

        [[nodiscard]] Engine* engine() const { return engine_; }
        [[nodiscard]] const Config& config() const { return config_; }

        DECL_SFINAE_METHOD_NO_ARGS(init)
        DECL_SFINAE_METHOD_FLOAT_ARG(update, delta)
        DECL_SFINAE_METHOD_NO_ARGS(finalize)

        void apply_deferred() {
            for (auto& action : deferred_actions_) {
                action(*static_cast<Derived*>(this));
            }
            deferred_actions_.clear();
        }

    protected:
        typedef std::function<void(Derived&)> DeferredAction;

        /**
         * @brief Отложенное действие подсистемы
         * @warning Должно вызываться только во владеющем потоке
         * @param action Callback-функция
         */
        void defer(DeferredAction action) {
            deferred_actions_.emplace_back(std::move(action));
        }

    private:
        Engine* const engine_;
        Config config_;
        std::vector<DeferredAction> deferred_actions_;
    };


    /**
     * @brief Специализация CRTP-класса подсистемы без config
     * @tparam Derived Тип класса наследника
     */
    template<typename Derived>
    class Subsystem<Derived, void>
    {
    public:
        explicit Subsystem(Engine* engine)
        : engine_(engine)
        {}

        [[nodiscard]] Engine* engine() const { return engine_; }

        DECL_SFINAE_METHOD_NO_ARGS(init)
        DECL_SFINAE_METHOD_FLOAT_ARG(update, delta)
        DECL_SFINAE_METHOD_NO_ARGS(finalize)

        void apply_deferred() {
            for (auto& action : deferred_actions_) {
                action(*static_cast<Derived*>(this));
            }
            deferred_actions_.clear();
        }

    protected:
        typedef std::function<void(Derived&)> DeferredAction;

        /**
         * @brief Отложенное действие подсистемы
         * @warning Должно вызываться только во владеющем потоке
         * @param action Callback-функция
         */
        void defer(DeferredAction action) {
            deferred_actions_.emplace_back(std::move(action));
        }

    private:
        std::vector<DeferredAction> deferred_actions_;
        Engine* const engine_;
    };

    /**
     * @brief Базовый класс для RAII обертки над сущностью, порождаемой подсистемой
     * @tparam SubsystemType Тип подсистемы
     */
    template<class SubsystemType>
    class SubsystemObject
    {
    public:
        [[nodiscard]] SubsystemType* subsystem() const { return subsystem_; }

    protected:
        explicit SubsystemObject(SubsystemType* subsystem)
        : subsystem_(subsystem)
        {}

    private:
        SubsystemType* const subsystem_;
    };
}
