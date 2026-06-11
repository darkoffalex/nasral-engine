#pragma once
#include <nasral/ecs/system.h>
#include <nasral/log/loggable.h>

namespace nasral::ecs{
    class Manager;
}

namespace nasral::res{
    class Manager;
}

namespace nasral::gfx
{
    class Manager;
    class System : public ecs::System<System, Manager>, public log::Loggable<System>
    {
    public:
        typedef std::unique_ptr<System> Ptr;
        explicit System(Manager* m);
        ~System();

        System(const System&) = delete;
        System& operator=(const System&) = delete;

        void on_init() const;
        void on_update(float delta) const;
        void on_finalize() const;

    protected:
        static void update_mtl_ubo(ecs::Manager* ecs, const Manager* gfx);
        static void update_mtl_handles(ecs::Manager* ecs, const res::Manager* res);
        static void update_mtl_textures(ecs::Manager* ecs, const Manager* gfx);
        static void update_obj_static_ubo(ecs::Manager* ecs, const Manager* gfx);
        static void update_obj_dynamic_ubo(ecs::Manager* ecs, const Manager* gfx);
        static void update_obj_mesh_handles(ecs::Manager* ecs, const res::Manager* res);
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::System, "GFX|ECS")
