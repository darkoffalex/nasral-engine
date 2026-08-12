#pragma once
#include <nasral/ecs/system.h>
#include <nasral/gfx/types.h>
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
        // Материалы
        void update_mtl_ubo() const;
        void update_mtl_handles() const;
        void update_mtl_textures() const;
        void update_mtl_destroy() const;

        // Пост-процессинг
        void update_pp_mtl_handles() const;

        // Объекты (UBO)
        void update_obj_static_ubo() const;
        void update_obj_dynamic_ubo() const;
        void update_obj_mesh_handles() const;

        // Источники света (UBO)
        void update_light_static_ubo() const;
        void update_light_dynamic_ubo() const;

        // Камеры (UBO)
        void update_cam_ubo() const;

        // Рендеринг
        void render_meshes() const;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(gfx::System, "GFX|ECS")
