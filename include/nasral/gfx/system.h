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

        void on_init() const;
        void on_update(float delta) const;
        void on_finalize() const;
        void on_render() const;

    protected:
        void update_mtl_ubo() const;
        void update_mtl_handles() const;
        void update_mtl_textures() const;
        void update_obj_static_ubo() const;
        void update_obj_dynamic_ubo() const;
        void update_obj_mesh_handles() const;
        void update_cam_ubo() const;

        void render_meshes() const;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::System, "GFX|ECS")
