#include "pch.h"
#include <nasral/scn/system.h>
#include <nasral/ecs/view.h>
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

        for (auto [e, spatial, state] : ecs->view<Spatial, UboState>(ecs::kMaskOf<Cam>))
        {
            if (state.dirty) continue;

            constexpr float rot_speed = 10.0f;

            spatial.rotation.y += dt * rot_speed;
            state.dirty = true;
        }
    }
}
