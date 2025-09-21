#pragma once
#include <nasral/logging/logger.h>
#include <nasral/ecs/ecs_manager.h>
#include <nasral/rendering/renderer.h>
#include <nasral/rendering/rendering_system.h>
#include <nasral/resources/resource_manager.h>
#include <nasral/resources/resource_system.h>

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
            ecs::EcsConfig ecs;
        };

        Engine();
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        bool initialize(const Config& config) noexcept;
        void update(float delta) noexcept;
        void shutdown() noexcept;

        [[nodiscard]] logging::Logger* logger() const {
            return logger_.get();
        }

        [[nodiscard]] ecs::EcsManager* ecs() const{
            return ecs_.get();
        }

        [[nodiscard]] rendering::Renderer* renderer() const {
            return renderer_.get();
        }

        [[nodiscard]] resources::ResourceManager* resource_manager() const {
            return resource_manager_.get();
        }

    private:
        bool initialized_ = false;
        logging::Logger::Ptr logger_;
        ecs::EcsManager::Ptr ecs_;
        rendering::Renderer::Ptr renderer_;
        rendering::RenderingSystem::Ptr rendering_system_;
        resources::ResourceManager::Ptr resource_manager_;
        resources::ResourceSystem::Ptr resource_system_;
    };
}
