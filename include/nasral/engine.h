#pragma once
#include <nasral/core_types.h>
#include <nasral/logging/logger.h>

namespace nasral
{
    class Engine
    {
    public:
        Engine();
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        bool initialize(const EngineConfig& config);
        void shutdown();

        [[nodiscard]] EngineContext context() const;

    private:
        std::unique_ptr<logging::Logger> logger_;
    };
}