#pragma once
#include <nasral/ecs/ecs_manager.h>
#include <nasral/rendering/rendering_types.h>
#include <nasral/rendering/renderer.h>

namespace nasral::rendering
{
    class RenderingSystem
    {
    public:
        typedef std::unique_ptr<RenderingSystem> Ptr;

        explicit RenderingSystem(Engine* engine);
        ~RenderingSystem();

        RenderingSystem(const RenderingSystem&) = delete;
        RenderingSystem& operator=(const RenderingSystem&) = delete;

        void update(float delta) const;
        void render() const;

    private:
        [[nodiscard]] ecs::EcsManager* ecs() const;
        [[nodiscard]] const logging::Logger* logger() const;
        [[nodiscard]] Renderer* renderer() const;

        void init_test_materials();
        void init_test_meshes();
        void init_test_cameras() const;
        void init_test_lights();

        //[[nodiscard]] static std::string default_tex_path(MaterialType m_type, TextureType t_type);
        [[nodiscard]] std::string_view valid_path(const std::string& path) const;

    protected:
        SafeHandle<Engine> engine_;
        std::vector<ecs::EntityId> materials_;
        std::vector<ecs::EntityId> meshes_;
        std::vector<ecs::EntityId> lights_;
    };
}