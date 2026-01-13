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
    {}

    void System::shutdown()
    {
        using Desc = res::comp::Descriptor;
        using Loaded = res::comp::Loaded;
        using Release = res::comp::Release;
        using Node = comp::Node;

        // Запросить освобождение загруженных ресурсов узлов
        auto* ecs = engine()->ecs();
        for (auto [e, n, r, l] : ecs->view<Node, Desc, Loaded>()){
            ecs->add_component_deferred<Release>(e);
        }
    }
}
