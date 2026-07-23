#include "pch.h"
#include <nasral/scn/system.h>
#include <nasral/scn/utils.h>
#include <nasral/inp/manager.h>
#include <nasral/scn/components.h>
#include <nasral/res/components.h>
#include <nasral/ecs/view.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    System::System(Manager* m) : ecs::System<System, Manager>(m)
    {
        activate_lights_.reserve(gfx::kMaxLights);
        deactivate_lights_.reserve(gfx::kMaxLights);
    }

    System::~System() = default;

    void System::on_init() const{
        log_info("ECS-system initialized");
    }

    void System::on_update(const float delta)
    {
        // Запросы ресурсов у узлов
        update_resource_requests();

        // Обработка ввода
        update_cam_input(delta);

        // Состояние источников света (активация/деактивация)
        update_light_states();

        // Уничтожение узлов
        update_mesh_destroy();
        update_light_destroy();
    }

    void System::on_finalize() const{
        update_mesh_destroy();
        update_light_destroy();
        log_info("ECS-system finalized");
    }

    void System::update_resource_requests() const
    {
        // TODO: Реализовать логику отбрасывания узлов
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

    void System::update_cam_input(const float delta) const
    {
        // Алиасы компонентов
        using Node       = NodeComponent;
        using Camera     = ViewComponent;
        using Spatial    = SpatialComponent;
        using Uniform    = gfx::UniformStateComponent;

        // Найти первую камеру
        const auto first_cam = engine()->ecs()->view<
            Node,
            Spatial,
            Camera,
            Uniform>().begin();

        // Если нет - выход
        if (!first_cam) return;

        // Внести изменения (если еще не внесены)
        if (auto [e, n, spatial, cam, uniform] = *first_cam; !uniform.is_dirty)
        {
            // Вектор перемещения (управление с клавиатуры)
            auto movement = engine()->inp()->get_movement_vector(
                inp::KeyCode::eA,
                inp::KeyCode::eD,
                inp::KeyCode::eW,
                inp::KeyCode::eS,
                inp::KeyCode::eSpace,
                inp::KeyCode::eC);

            // При зажатой ЛКМ обновлять поворот
            if (engine()->inp()->is_mouse_btn_pressed(inp::MouseButton::eLeft))
            {
                constexpr float rot_speed = 0.1f;
                spatial.rotation.y += engine()->inp()->mouse_delta().x * -rot_speed;
                spatial.rotation.x += engine()->inp()->mouse_delta().y * -rot_speed;
                uniform.is_dirty = true;
            }

            // Если есть перемещения (при вводе с клавиатуры) - обновлять положение
            if (glm::length2(movement) > 0.0f)
            {
                constexpr float move_speed = 2.5f;

                glm::vec3 oriented_movement = calc_rot_movement(
                    {movement.x, movement.z},
                    spatial.rotation);

                spatial.position += (oriented_movement + glm::vec3(0.0f, movement.y, 0.0f))
                    * move_speed
                    * delta;

                uniform.is_dirty = true;
            }
        }
    }

    void System::update_mesh_destroy() const
    {
        // Алиасы компонентов
        using Node      = NodeComponent;
        using Mesh      = MeshComponent;
        using UniformId = gfx::UniformIndexComponent;
        using Destroy   = ecs::DestroyComponent;

        // Пройти по всем mesh-ам, помеченным к удалению.
        // Предполагается, что в конце update-итерации сущность будет удалена (повторной обработки не случится)
        for (auto [e, n, m, ui, d_tag] : engine()->ecs()->view<
            Node,
            Mesh,
            UniformId,
            Destroy>())
        {
            engine()->gfx()->object_ubo_ids().release(ui.index);
        }
    }

    void System::update_light_destroy() const
    {
        // Алиасы компонентов
        using Node      = NodeComponent;
        using Light     = LightComponent;
        using UniformId = gfx::UniformIndexComponent;
        using Destroy   = ecs::DestroyComponent;

        // Пройти по всем источникам света, помеченным к удалению.
        // Предполагается, что в конце update-итерации сущность будет удалена (повторной обработки не случится)
        for (auto [e, n, l, ui, d_tag] : engine()->ecs()->view<
            Node,
            Light,
            UniformId,
            Destroy>())
        {
            engine()->gfx()->light_ubo_ids().release(ui.index);
        }
    }

    void System::update_light_states()
    {
        // Алиасы компонентов
        using Node       = NodeComponent;
        using Light      = LightComponent;
        using UniformId  = gfx::UniformIndexComponent;
        using Activate   = ecs::ActivateComponent;
        using Deactivate = ecs::DeactivateComponent;

        // Пройти по источникам, которые необходимо активировать, и сформировать список
        activate_lights_.clear();
        for (auto [e, n, l, ui, a_tag] : engine()->ecs()->view<
            Node,
            Light,
            UniformId,
            Activate>())
        {
            activate_lights_.push_back(ui.index);
            engine()->ecs()->remove_components<Activate>(e);
        }

        // Активировать источники (если они есть)
        if (!activate_lights_.empty()){
            engine()->gfx()->update_light_states_unsafe(activate_lights_, true);
        }

        // Пройтись по источникам, которые необходимо деактивировать, и сформировать список
        deactivate_lights_.clear();
        for (auto [e, n, l, ui, d_tag] : engine()->ecs()->view<
            Node,
            Light,
            UniformId,
            Deactivate>())
        {
            deactivate_lights_.push_back(ui.index);
            engine()->ecs()->remove_components<Deactivate>(e);
        }

        // Деактивировать источники (если они есть)
        if (!deactivate_lights_.empty()){
            engine()->gfx()->update_light_states_unsafe(deactivate_lights_, false);
        }
    }
}
