#pragma once

#include <nasral/core/subsystem.h>
#include <nasral/scn/types.h>
#include <nasral/log/loggable.h>
#include <nasral/ecs/entity.h>
#include <nasral/evt/types.h>

namespace nasral::scn
{
    class Manager final : public core::Subsystem<Config>, public log::Loggable<Manager>
    {
    public:
        Manager(Engine* e, const Config& cfg);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

    private:
        void on_project_loaded(const evt::Arg& arg) const;

    protected:
        // Корень сцены
        ecs::EntityId root_;

        // События
        evt::ListenerHandle evt_h_proj_load_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(scn::Manager)