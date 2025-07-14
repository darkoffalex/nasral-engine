#pragma once
#include <nasral/logging/logger.h>
#include <nasral/resources/resource_manager.h>
#include <nasral/rendering/renderer.h>

namespace nasral
{
    class Engine
    {
    public:
        struct Config
        {
            logging::LoggingConfig log;
            resources::ResourceConfig resources;
            rendering::RenderingConfig rendering;
        };

        Engine();
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        bool initialize(const Config& config);
        void update(float delta) const;
        void shutdown();

        [[nodiscard]] const logging::Logger* logger() const {
            return logger_.get();
        }

        [[nodiscard]] const resources::ResourceManager* resource_manager() const {
            return resource_manager_.get();
        }

        [[nodiscard]] const rendering::Renderer* renderer() const {
            return renderer_.get();
        }

    private:
        logging::Logger::Ptr logger_;
        resources::ResourceManager::Ptr resource_manager_;
        rendering::Renderer::Ptr renderer_;
    };
}