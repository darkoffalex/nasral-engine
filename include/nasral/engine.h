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

        class TestNode
        {
        public:
            explicit TestNode(const Engine* engine);
            ~TestNode() = default;

            void request_resources();
            void release_resources();
            void render(const rendering::Renderer::Ptr& renderer, size_t obj_index) const;
            void set_position(const glm::vec3& position, bool update_mat = true);
            void set_rotation(const glm::vec3& rotation, bool update_mat = true);
            void set_scale(const glm::vec3& scale, bool update_mat = true);

            [[nodiscard]] bool is_pending_ubo_update(bool reset = false);
            [[nodiscard]] const rendering::ObjectUniforms& uniforms() const { return uniforms_; }

        private:
            void update_matrix();

        protected:
            SafeHandle<Engine> engine_;

            resources::Ref material_ref_;
            resources::Ref mesh_ref_;
            resources::Ref texture_ref_;
            rendering::Handles::Mesh mesh_handles_;
            rendering::Handles::Material material_handles_;
            rendering::Handles::Texture texture_handles_;

            glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::vec3 rotation_ = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::vec3 scale_ = glm::vec3(1.0f, 1.0f, 1.0f);
            rendering::ObjectUniforms uniforms_ = {};
            bool pending_ubo_update_ = false;
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
        rendering::CameraUniforms camera_uniforms_;
    };
}
