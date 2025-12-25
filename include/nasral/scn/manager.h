#pragma once

#include <nasral/core/subsystem.h>
#include <nasral/scn/types.h>
#include <nasral/log/loggable.h>
#include <nasral/ecs/entity.h>

namespace nasral::scn
{
    class Manager final : public core::Subsystem<Config>, public log::Loggable<Manager>
    {
    public:
        Manager(Engine* e, const Config& cfg);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

    protected:
        ecs::EntityId root_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(scn::Manager)