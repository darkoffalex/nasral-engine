#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/gfx/manager.h>
#include <nasral/gfx/entities/material.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/view.h>
#include <nasral/ecs/utils.h>
#include <nasral/res/components.h>
#include <nasral/scn/components.h>
#include <nasral/res/objects/texture.h>
#include <nasral/res/objects/mesh.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    System::System(Manager* m) : ecs::System<System, Manager>(m){}

    System::~System() = default;

    void System::on_init() const{
        log_info("ECS-system initialized");
    }

    void System::on_update([[maybe_unused]] const float delta) const
    {
        // Материалы (основные)
        update_mtl_ubo();
        update_mtl_handles();
        update_mtl_textures();
        update_mtl_destroy();

        // Объекты
        update_obj_static_ubo();
        update_obj_dynamic_ubo();
        update_obj_mesh_handles();

        // Источники света
        update_light_static_ubo();
        update_light_dynamic_ubo();

        // Камеры
        update_cam_ubo();
    }
}