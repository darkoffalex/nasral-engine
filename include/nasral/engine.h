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
            enum UpdateFlags : uint32_t
            {
                eNone       = 0,
                eTransform  = 1 << 0,
                eMaterial   = 1 << 1,
                eTextures   = 1 << 2,
                eAll = eTransform | eMaterial | eTextures
            };

            friend class Engine;
            explicit TestNode(const Engine* engine, uint32_t index);
            ~TestNode();

            void request_resources();
            void release_resources();
            void update(const rendering::Renderer::Ptr& renderer);
            void render(const rendering::Renderer::Ptr& renderer) const;

            void set_position(const glm::vec3& position);
            void set_rotation(const glm::vec3& rotation);
            void set_scale(const glm::vec3& scale);

        protected:
            SafeHandle<const Engine> engine_;
            uint32_t obj_index_ = 0;

            resources::Ref material_ref_;
            resources::Ref mesh_ref_;
            resources::Ref texture_ref_;

            rendering::Handles::Mesh mesh_handles_;
            rendering::Handles::Material material_handles_;
            rendering::Handles::Texture texture_handles_;
            
            glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::vec3 rotation_ = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::vec3 scale_ = glm::vec3(1.0f, 1.0f, 1.0f);
            UpdateFlags pending_updates_ = UpdateFlags::eAll;
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
