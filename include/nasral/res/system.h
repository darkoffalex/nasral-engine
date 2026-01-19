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
        bool validate(const ecs::EntityId& entity, const IResource* resource, bool add_err = true) const;

        void update_requested() const;
        void update_released() const;

        void on_texture_loaded(const ecs::EntityId& entity, IResource* res, gfx::TextureType type) const;
        void on_material_loaded(const ecs::EntityId& entity, IResource* res) const;
        void on_mesh_loaded(const ecs::EntityId& entity, IResource* res) const;
        void on_scene_loaded(const ecs::EntityId& entity, IResource* res) const;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(res::System)