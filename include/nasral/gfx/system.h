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

        void init();
        void update(float delta);
        void finalize();

    protected:
        void update_materials();
        void update_objects();
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::System)
