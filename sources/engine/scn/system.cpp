#include "pch.h"
#include <nasral/scn/system.h>
#include <nasral/ecs/view.h>
#include <nasral/scn/manager.h>
#include <nasral/scn/utils.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    System::System(Engine* engine) : ecs::System<System>(engine)
    {}

    System::~System()
    = default;

    void System::init()
    {}

    void System::update([[maybe_unused]] const float dt)
    {
        update_obj_transforms(dt);
        update_camera_transform(dt);
    }

    void System::shutdown()
    {
        using Desc = res::comp::Descriptor;
        using Loaded = res::comp::Loaded;
        using Release = res::comp::Release;
        using Node = comp::Node;
        using Spatial = comp::Spatial;
        using Cam = comp::Camera;
        using Light = comp::Light;
        using UboIdx = gfx::comp::UniformIndex;

        auto* ecs = engine()->ecs();
        auto* gfx = engine()->renderer();

        // Запросить освобождение загруженных ресурсов узлов
        for (auto [e, n, r, l] : ecs->view<Node, Desc, Loaded>()){
            ecs->add_component_deferred<Release>(e);
        }

        // Освободить UBO индексы у пространственных узлов
        for (auto [e, n, s, u] : ecs->view<Node, Spatial, UboIdx>(ecs::kMaskOf<Cam>))
        {
            if (ecs->has_component<Light>(e)){
                gfx->light_ids().release(u.index);
            }
            else{
                gfx->object_ids().release(u.index);
            }
        }
    }

    void System::update_obj_transforms(const float dt) const
    {
        auto* ecs = engine()->ecs();

        using Spatial = comp::Spatial;
        using UboState = gfx::comp::UniformState;
        using Cam = comp::Camera;
        using Light = comp::Light;

        for (auto [e, spatial, state] : ecs->view<Spatial, UboState>(ecs::kMaskOf<Cam, Light>))
        {
            if (state.dirty) continue;

            constexpr float rot_speed = 10.0f;

            spatial.rotation.y += dt * rot_speed;
            state.dirty = true;
        }
    }

    void System::update_camera_transform([[maybe_unused]] const float dt) const
    {
        using Spatial = comp::Spatial;
        using UboState = gfx::comp::UniformState;
        using Cam = comp::Camera;

        auto* ecs = engine()->ecs();
        const auto* input = engine()->input();

        for (auto [e, spatial, cam, state] : ecs->view<Spatial, Cam, UboState>())
        {
            if (state.dirty) continue;

            auto movement = input->get_movement_vector(
                inp::KeyCode::eA,
                inp::KeyCode::eD,
                inp::KeyCode::eW,
                inp::KeyCode::eS,
                inp::KeyCode::eSpace,
                inp::KeyCode::eC);

            if (input->is_mouse_btn_pressed(inp::MouseButton::eLeft))
            {
                constexpr float rot_speed = 0.1f;
                spatial.rotation.y += input->mouse_delta().x * -rot_speed;
                spatial.rotation.x += input->mouse_delta().y * -rot_speed;
                state.dirty = true;
            }

            if (glm::length2(movement) > 0.0f)
            {
                constexpr float move_speed = 2.5f;

                glm::vec3 oriented_movement = snc::calc_rot_movement(
                    {movement.x, movement.z},
                    spatial.rotation);

                spatial.position += (oriented_movement + glm::vec3(0.0f, movement.y, 0.0f))
                    * move_speed
                    * dt;

                state.dirty = true;
            }
        }
    }
}
