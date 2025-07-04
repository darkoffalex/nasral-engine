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
        void update(float delta) const;
        void shutdown();

        [[nodiscard]] const logging::Logger* logger() const {
            return logger_.get();
        }

    private:
        logging::Logger::Ptr logger_;
        resources::ResourceManager::Ptr resource_manager_;
    };
}