#pragma once
#include <nasral/ecs/system.h>
#include <nasral/log/loggable.h>

namespace nasral::gfx
{
    class System final : public ecs::System<System>, public log::Loggable<System>
    {
    public:
        using Ptr = std::unique_ptr<System>;
        explicit System(Engine* engine);
        ~System();

        System(const System&) = delete;
        System& operator=(const System&) = delete;

        void init();
        void update(float dt);
        void shutdown();
        void render() const;

    private:
        void update_material_settings() const;
        void update_material_textures() const;
        void update_objects_uniforms() const;
        void update_lights_uniforms() const;
        void update_cam_uniforms() const;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(gfx::System)