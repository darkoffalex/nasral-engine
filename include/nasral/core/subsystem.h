#pragma once

namespace nasral
{
    class Engine;
}

namespace nasral::core
{
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

    protected:
        Engine* const engine_;
        Config config_;
    };
}