#pragma once
#include <nasral/ecs/manager.h>
#include <nasral/ecs/system.h>
#include <nasral/log/loggable.h>

namespace nasral::res
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
        static void process_requests(ecs::Manager* ecs, Manager* res);
        static void process_loadings(ecs::Manager* ecs);
        static void process_releases(ecs::Manager* ecs, Manager* res);
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(res::System, "RES|ECS")
