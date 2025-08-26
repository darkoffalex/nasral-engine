#pragma once
#include <nasral/logging/logger.h>
#include <nasral/resources/resource_manager.h>
#include <nasral/rendering/renderer.h>
#include <nasral/rendering/mesh_instance.h>

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

        class TestNode
        {
        public:
            friend class rendering::Renderer;
            struct SpatialSettings
            {
                glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
                glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f);
                glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
                bool updated = true;
            };

            TestNode() = default;
            explicit TestNode(const Engine* engine);
            ~TestNode();

            void request_resources();
            void release_resources();
            void update();
            void render() const;

            void set_position(const glm::vec3& position);
            void set_rotation(const glm::vec3& rotation);
            void set_scale(const glm::vec3& scale);
            void set_material(uint32_t index);
            void set_mesh(rendering::MeshInstance instance);

            [[nodiscard]] const SpatialSettings& spatial_settings() const {return spatial_settings_;}
            [[nodiscard]] rendering::MeshInstance& mesh_instance() {return mesh_;}

        private:
            SafeHandle<const Engine> engine_;
            uint32_t obj_index_ = 0;
            uint32_t material_index_ = 0;
            rendering::MeshInstance mesh_ = {};
            SpatialSettings spatial_settings_ = {};
        };

        class LightSource
        {
        public:
            friend class rendering::Renderer;

            LightSource() = default;
            explicit LightSource(const Engine* engine);
            ~LightSource();

            void update();

            void set_position(const glm::vec3& position);
            void set_color(const glm::vec3& color);
            void set_intensity(float intensity);
            void set_radius(float radius);
            void set_active(bool active);

        private:
            SafeHandle<const Engine> engine_;
            uint32_t light_index_ = 0;
            rendering::LightUniforms light_uniforms_ = {};
            bool settings_updated_ = false;
            bool state_updated_ = false;
            bool active_ = false;
        };

        Engine();
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        bool initialize(const Config& config) noexcept;
        void update(float delta) noexcept;
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

    protected:
        // Только для тестирования
        void init_test_scene();

    private:
        logging::Logger::Ptr logger_;
        resources::ResourceManager::Ptr resource_manager_;
        rendering::Renderer::Ptr renderer_;

        // Только для тестирования
        std::vector<TestNode> test_scene_nodes_;
        std::vector<LightSource> test_light_sources_;
        rendering::CameraUniforms camera_uniforms_;
    };
}
