#pragma once
#include <nasral/core_types.h>
#include <nasral/logging/logger.h>
#include <nasral/resources/resource_manager.h>

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
        std::unique_ptr<resources::ResourceManager> resource_manager_;
    };
}