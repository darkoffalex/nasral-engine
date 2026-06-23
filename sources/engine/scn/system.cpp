#include "pch.h"
#include <nasral/scn/system.h>
#include <nasral/scn/components.h>
#include <nasral/res/components.h>
#include <nasral/ecs/view.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    System::System(Manager* m) : ecs::System<System, Manager>(m){}

    System::~System() = default;

    void System::on_init() const{
        log_info("ECS-system initialized");
    }

    void System::on_update([[maybe_unused]] float delta) const{
        update_resource_requests();
    }

    void System::on_finalize() const{
        log_info("ECS-system finalized");
    }

    void System::update_resource_requests() const
    {
        // TODO: Реализовать логигу отбрасывания узлов
        // Временное решение.
        // Система запрашивает ресурсы узлов сцены по надобности.
        // Сейчас запрашиваются ресурсы всех узлов.
        // В перспективе будет реализована более сложная логика (отброса/запроса узлов)

        // Алиасы компонентов
        using Node      = NodeComponent;
        using Resources = res::ResourcesComponent;
        using Request   = res::RequestComponent;
        using Loading   = res::LoadingComponent;
        using Loaded    = res::LoadedComponent;

        // Пройти по всем сущностям с компонентами:
        // - Узел сцены
        // - Ресурсы
        // Где нет компонентов:
        // - Запрос
        // - Загрузка
        // - Загружен
        for (auto [e, n, res] : engine()->ecs()->view<Node, Resources>(ecs::kMaskOf<Request, Loading, Loaded>))
        {
            engine()->ecs()->add_components<Request>(e, {});
        }
    }
}
