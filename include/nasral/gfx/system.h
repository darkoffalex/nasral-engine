#pragma once
#include <nasral/ecs/system.h>
#include <nasral/log/loggable.h>

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

        void on_init();
        void on_update(float delta);
        void on_finalize();

    protected:
        void process_materials_update();
        void process_objects_update();
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::System, "GFX|ECS")
