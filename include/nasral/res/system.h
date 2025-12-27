#pragma once
#include <nasral/ecs/system.h>
#include <nasral/ecs/entity.h>
#include <nasral/res/resource.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>

namespace nasral::res
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
        void update_generic_resources(float dt) const;
        void update_texture_resources(float dt) const;

        void on_generic_loaded(const ecs::EntityId& entity, IResource* res) const;
        void on_texture_loaded(const ecs::EntityId& entity, IResource* res, gfx::TextureType type) const;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(res::System)