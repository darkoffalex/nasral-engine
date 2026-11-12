#pragma once
#include <nasral/ecs/system.h>
#include <nasral/log/loggable.h>

namespace nasral::scn
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
        void on_update(float delta);
        void on_finalize() const;

    protected:
        void update_resource_requests() const;
        void update_cam_input(float delta) const;
        void update_light_input(float delta) const;
        void update_mesh_destroy() const;
        void update_light_destroy() const;
        void update_light_states();

    private:
        std::vector<uint32_t> activate_lights_;
        std::vector<uint32_t> deactivate_lights_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(scn::System, "SCN|ECS")