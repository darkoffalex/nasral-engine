#pragma once
#include <nasral/logging/logger.h>
#include <nasral/resources/resource_manager.h>
#include <nasral/rendering/renderer.h>

#include "resources/mesh.h"

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

        // Только для тестирования
        struct TestScene
        {
            typedef std::unique_ptr<TestScene> Ptr;

            resources::Ref material_ref;
            resources::Ref mesh_ref;

            rendering::Handles::Mesh mesh_handles;
            rendering::Handles::Material material_handles;

            explicit TestScene(const resources::ResourceManager::Ptr& resource_manager);
            ~TestScene();
            void render(const rendering::Renderer::Ptr& renderer) const;
        };

        Engine();
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        bool initialize(const Config& config) noexcept;
        void update(float delta) const noexcept;
        void shutdown() noexcept;

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

        // Только для тестирования
        TestScene::Ptr test_scene_;
    };
}
