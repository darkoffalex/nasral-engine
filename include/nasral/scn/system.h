#pragma once
#include <nasral/ecs/system.h>
#include <nasral/res/resource.h>
#include <nasral/log/loggable.h>

namespace nasral::scn
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

    private:
        void update_obj_transforms(float dt) const;
        void update_camera_transform(float dt) const;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(scn::System)