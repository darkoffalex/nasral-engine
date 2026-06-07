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
        static void process_materials_dirty_ubo(ecs::Manager* ecs, Manager* gfx);
        static void process_materials_dirty_handles(ecs::Manager* ecs, res::Manager* res);
        static void process_materials_dirty_textures(ecs::Manager* ecs, Manager* gfx);
        static void process_objects_dirty_ubo(ecs::Manager* ecs, Manager* gfx);
        static void process_objects_dirty_handles(ecs::Manager* ecs, res::Manager* res);
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::System, "GFX|ECS")
