#include "pch.h"
#include <nasral/res/system.h>
#include <nasral/res/manager.h>
#include <nasral/res/components.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/view.h>
#include <nasral/engine.h>

namespace nasral::res
{
    System::System(Manager* m) : ecs::System<System, Manager>(m){}

    System::~System() = default;

    void System::on_init() const{
        log_info("ECS-system initialized");
    }

    void System::on_update([[maybe_unused]] const float delta) const{
        auto* ecs = engine()->ecs();
        auto* res = subsystem();

        update_requests(ecs, res);
        update_loadings(ecs);
        update_releases(ecs, res);
        // update_references(ecs);
    }

    void System::on_finalize() const
    {
        auto* ecs = subsystem()->engine()->ecs();
        auto* res = subsystem();

        // Пройти по всем сущностям с компонентами:
        // - Список ресурсов
        for (auto [e, resources] : ecs->view<ResourcesComponent>())
        {
            // Выполнить освобождение
            for (size_t i = 0; i < kResListComponentSize; ++i)
            {
                if (resources.ids[i] == kInvalidResourceId) continue;
                if (!resources.active[i]) continue;

                if (resources.statuses[i] != Status::eUnloaded){
                    resources.statuses[i] = Status::eUnloaded;
                    res->release(resources.ids[i]);
                }
            }

            // Убрать из списка освобождаемых
            ecs->remove_components<LoadedComponent, ReleaseComponent>(e);
        }

        log_info("ECS-system finalized");
    }

    void System::update_requests(ecs::Manager* ecs, Manager* res)
    {
        // Пройти по всем сущностям с компонентами:
        // - Список ресурсов
        // - Запрос ресурсов
        for (auto [e, resources, req_tag] : ecs->view<ResourcesComponent, RequestComponent>())
        {
            // Убрать из списка запросов
            ecs->remove_components<RequestComponent>(e);
            // Добавить в список загружаемых
            ecs->add_components<LoadingComponent>(e, {});

            // Выполнить запросы активных ресурсов
            for (size_t i = 0; i < kResListComponentSize; ++i)
            {
                if (resources.active[i] == false || resources.statuses[i] != Status::eUnloaded){
                    continue;
                }

                if (resources.ids[i] == kInvalidResourceId){
                    resources.statuses[i] = Status::eError;
                    continue;
                }

                res->request(resources.ids[i], [ecs, res, entity = e, i](const Resource* resource)
                {
                    assert(res != nullptr);
                    // Если entity уничтожена - освободить ресурс (предотвращение висячих ссылок)
                    if (!ecs->is_valid(entity)){
                        res->release(resource->id());
                        return;
                    }

                    // Обновить статус
                    auto& rc = ecs->get_component<ResourcesComponent>(entity);
                    rc.statuses[i] = resource->status();
                });
            }
        }
    }

    void System::update_loadings(ecs::Manager* ecs)
    {
        // Пройти по всем сущностям с компонентами:
        // - Список ресурсов
        // - Загружается
        // И без компонентов:
        // - Загружен
        for (auto [e, resources, l_tag] : ecs->view<ResourcesComponent, LoadingComponent>(ecs::kMaskOf<LoadedComponent>))
        {
            // Проверить готовность и ошибки
            bool all_done = true;
            bool has_error = false;
            for (size_t i = 0; i < kResListComponentSize; ++i){
                if (!resources.active[i]) continue;
                if (resources.statuses[i] == Status::eUnloaded){
                    all_done = false;
                }else if (resources.statuses[i] == Status::eError){
                    has_error = true;
                }
            }

            // После готовности убрать из "загружаемых" и переместить в соответствующие списки
            if (all_done){
                ecs->remove_components<LoadingComponent>(e);
                if (has_error){
                    ecs->add_components<LoadedComponent, ErrorComponent>(e, {}, {});
                }else{
                    ecs->add_components<LoadedComponent>(e, {});
                }
            }
        }
    }

    void System::update_releases(ecs::Manager* ecs, Manager* res)
    {
        // Пройти по всем сущностям с компонентами:
        // - Список ресурсов
        // - Загружен
        // - Освободить
        for (auto [e, resources, l_tag, r_tag] : ecs->view<ResourcesComponent, LoadedComponent, ReleaseComponent>())
        {
            // Убрать из списка освобождаемых
            ecs->remove_components<LoadedComponent, ReleaseComponent>(e);

            // Выполнить освобождение активных ресурсов
            for (size_t i = 0; i < kResListComponentSize; ++i){
                if (resources.ids[i] == kInvalidResourceId) continue;
                if (!resources.active[i]) continue;

                if (resources.statuses[i] != Status::eUnloaded){
                    resources.statuses[i] = Status::eUnloaded;
                    res->release(resources.ids[i]);
                }
            }
        }
    }

    void System::update_references(ecs::Manager* ecs)
    {
        // Пройти по всем сущностям с компонентами:
        // - Список ресурсов
        // - Счётчик ссылок
        // - Ссылки изменились
        // И без компонентов:
        // - Запросить
        // - Освободить
        // - Загружается
        for (auto [e, res, refs, rc_tag] : ecs->view<
            ResourcesComponent,
            ecs::RefsCountComponent,
            ecs::RefsChangedComponent>(ecs::kMaskOf<RequestComponent, ReleaseComponent, LoadingComponent>))
        {
            if (refs.count == 0){
                if (ecs->has<LoadedComponent>(e)){
                    ecs->add_components<ReleaseComponent>(e, {});
                }
            }else{
                if (!ecs->has<LoadedComponent>(e)){
                    ecs->add_components<RequestComponent>(e, {});
                }
            }
        }
    }
}
