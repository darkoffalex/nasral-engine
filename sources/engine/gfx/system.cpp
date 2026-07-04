#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/gfx/manager.h>
#include <nasral/gfx/objects/material.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/view.h>
#include <nasral/ecs/utils.h>
#include <nasral/res/components.h>
#include <nasral/scn/components.h>
#include <nasral/res/objects/material.h>
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
        // Материалы
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

    void System::on_finalize() const{
        update_mtl_destroy();
        log_info("ECS-system finalized");
    }

    void System::on_render() const
    {
        render_meshes();
    }

    void System::update_mtl_ubo() const
    {
        // Алиасы компонентов
        using Settings  = MaterialSettingsComponent;
        using UniformId = UniformIndexComponent;
        using Dirty     = DirtyUnformComponent;

        // Пройти по всем сущностям с компонентами:
        // - Настройки материала
        // - Uniform index
        // - Грязный (не обновленный) uniform
        for (auto [e, ms, ui, du_tag] : engine()->ecs()->view<Settings, UniformId, Dirty>())
        {
            // Для всех вариаций настроек материала
            std::visit([&, index = ui.index](auto&& uniforms){
                engine()->gfx()->update_mat_uniforms(uniforms, index);
            }, ms.uniforms);

            // Обновлено
            engine()->ecs()->remove_components<Dirty>(e);
        }
    }

    void System::update_mtl_handles() const
    {
        // Алиасы компонентов
        using Handles   = MaterialHandlesComponent;
        using Resources = res::ResourcesComponent;
        using Dirty     = DirtyHandlesComponent;
        using Loaded    = res::LoadedComponent;

        // Алиасы для индексов ресурсов
        using ResIndices = MaterialInstance::ResIndices;

        // Пройти по всем сущностям с компонентами:
        // - Handles материала
        // - Список ресурсов
        // - Грязные (не обновленные) handles
        // - Ресурсы загружены
        for (auto [e, mh, rsc, d_tag, l_tag] : engine()->ecs()->view<Handles, Resources, Dirty, Loaded>())
        {
            // Материал должен быть загружен
            if (kDebugBuild){
                assert(rsc.active[ResIndices::eBaseMaterial]);
                assert(rsc.ids[ResIndices::eBaseMaterial] != res::kInvalidResourceId);
            }

            // Ресурс материала (должен быть доступен)
            const auto* mat_res = engine()->res()->get<res::Material>(rsc.ids[ResIndices::eBaseMaterial]);
            assert(mat_res != nullptr && "Bad material");
            // Если загружен - обновить handles, если нет - fallback
            if (mat_res->status() == res::Status::eLoaded){
                mh.material = mat_res->render_handles();
            }else{
                // TODO: Fallback
            }

            // Итерация по типам текстур
            for (const auto type : magic_enum::enum_values<TextureType>()){
                if (type == TextureType::TOTAL) continue;
                const auto res_index = MaterialInstance::kTexResMap[type];
                // Если текстура используется
                if (rsc.active[res_index]){
                    assert(rsc.ids[res_index] != res::kInvalidResourceId);
                    const auto* tex_res = engine()->res()->get<res::Texture>(rsc.ids[res_index]);
                    assert(tex_res != nullptr && "Bad texture");
                    if (tex_res->status() == res::Status::eLoaded){
                        mh.textures[type] = tex_res->render_handles();
                    }
                    else{
                        // TODO: Fallback
                    }
                }
                else{
                    mh.textures[type] = {};
                }
            }

            // Обновлено
            engine()->ecs()->remove_components<Dirty>(e);
        }
    }

    void System::update_mtl_textures() const
    {
        // Алиасы компонентов
        using Handles       = MaterialHandlesComponent;
        using Settings      = MaterialSettingsComponent;
        using UniformId     = UniformIndexComponent;
        using DirtyHandles  = DirtyHandlesComponent;
        using DirtyTextures = DirtyTexturesComponent;

        // Пройти по всем сущностям с компонентами:
        // - Handles материала
        // - Настройки материала
        // - Uniform index
        // - Грязные (не обновленные) текстуры
        // Где нет компонентов:
        // - Грязные (не обновленные) handles
        for (auto [e, mh, ms, ui, dt_tag] : engine()->ecs()->view<
            Handles,
            Settings,
            UniformId,
            DirtyTextures>(ecs::kMaskOf<DirtyHandles>))
        {
            // Итерация по типам текстур
            for (const auto type : magic_enum::enum_values<TextureType>()){
                if (type == TextureType::TOTAL) continue;
                if (!mh.textures[type]) continue;

                engine()->gfx()->update_mat_textures({
                    type,
                    ms.samplers[type],
                    mh.textures[type]
                }, ui.index);
            }

            // Обновлено
            engine()->ecs()->remove_components<DirtyTextures>(e);
        }
    }

    void System::update_mtl_destroy() const
    {
        // Алиасы компонентов
        using Material  = MaterialSettingsComponent;
        using UniformId = UniformIndexComponent;
        using Destroy   = ecs::DestroyComponent;

        // Пройти по всем сущностям с компонентами
        // - Параметры материала
        // - Uniform index
        // - Уничтожение
        // Внимание: ожидается, что в конце полной итерации update сущность удаляется (что предотвратит повторную обработку)
        for (auto [e, ms, ui, d_tag] : engine()->ecs()->view<Material, UniformId, Destroy>())
        {
            engine()->gfx()->material_ubo_ids().release(ui.index);
        }
    }

    void System::update_obj_static_ubo() const
    {
        // Алиасы компонентов
        using Spatial   = scn::SpatialComponent;
        using UniformId = UniformIndexComponent;
        using Render    = RenderComponent;
        using Dirty     = DirtyUnformComponent;

        // Пройти по всем сущностям с компонентами:
        // - Пространственные параметры
        // - Uniform index
        // - Рендеринг
        // - Грязный (не обновленный) UBO
        for (auto [e, sp, ui, r_tag, d_tag] : engine()->ecs()->view<Spatial, UniformId, Render, Dirty>())
        {
            // Вычислить матрицы
            uniforms::Object uniforms = {};
            auto& model = uniforms.model;
            auto& normals = uniforms.normals;
            model = glm::translate(model, sp.position);
            model = glm::rotate(model, glm::radians(sp.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(sp.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(sp.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, sp.scale);
            normals = glm::transpose(glm::inverse(glm::mat3(model)));

            // Обновить матрицы для объекта
            engine()->gfx()->update_obj_uniforms(uniforms, ui.index);

            // Обновлено
            engine()->ecs()->remove_components<Dirty>(e);
        }
    }


    void System::update_obj_dynamic_ubo() const
    {
        // Алиасы компонентов
        using Spatial   = scn::SpatialComponent;
        using UniformId = UniformIndexComponent;
        using State     = UniformStateComponent;
        using Render    = RenderComponent;

        // Пройти по всем сущностям с компонентами:
        // - Пространственные параметры
        // - Uniform index
        // - Состояние UBO
        // - Рендеринг
        for (auto [e, sp, ui, state, r_tag] : engine()->ecs()->view<Spatial, UniformId, State, Render>())
        {
            if (!state.is_dirty) continue;

            // Вычислить матрицы
            uniforms::Object uniforms = {};
            auto& model = uniforms.model;
            auto& normals = uniforms.normals;
            model = glm::translate(model, sp.position);
            model = glm::rotate(model, glm::radians(sp.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(sp.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(sp.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, sp.scale);
            normals = glm::transpose(glm::inverse(glm::mat3(model)));

            // Обновить матрицы для объекта
            engine()->gfx()->update_obj_uniforms(uniforms, ui.index);

            // Обновлено
            state.is_dirty = false;
        }
    }

    void System::update_obj_mesh_handles() const
    {
        // Алиасы компонентов
        using Handles   = MeshHandlesComponent;
        using Resources = res::ResourcesComponent;
        using Dirty     = DirtyHandlesComponent;
        using Loaded    = res::LoadedComponent;

        // Пройти по всем сущностям с компонентами:
        // - Handles меша
        // - Список ресурсов
        // - Грязные (не обновленные) handles
        // - Ресурсы загружены
        for (auto [e, mh, rsc, d_tag, l_tag] : engine()->ecs()->view<Handles, Resources, Dirty, Loaded>())
        {
            // Меш должен быть загружен
            if (kDebugBuild){
                assert(rsc.active[0]);
                assert(rsc.ids[0] != res::kInvalidResourceId);
            }

            // Ресурс материала (должен быть доступен)
            const auto* mesh_res = engine()->res()->get<res::Mesh>(rsc.ids[0]);
            assert(mesh_res != nullptr && "Bad mesh resource");

            // Если загружен - обновить handles, если нет - fallback
            if (mesh_res->status() == res::Status::eLoaded){
                mh.mesh = mesh_res->render_handles();
            }else{
                // TODO: Fallback
            }

            // Обновлено
            engine()->ecs()->remove_components<Dirty>(e);
        }
    }

    void System::update_light_static_ubo() const
    {
        // Алиасы компонентов
        using Spatial   = scn::SpatialComponent;
        using Light     = scn::LightComponent;
        using UniformId = UniformIndexComponent;
        using Dirty     = DirtyUnformComponent;

        // Пройти по всем сущностям с компонентами:
        for (auto [e, sp, l, ui, d_tag] : engine()->ecs()->view<Spatial, Light, UniformId, Dirty>())
        {
            // Параметры источника
            uniforms::LightSettings uniforms = {};
            uniforms.position = {sp.position.x, sp.position.y, sp.position.z, 1.0f};
            uniforms.direction = {sp.rotation.x, sp.rotation.y, sp.rotation.z, 0.0f};
            uniforms.color = l.color;
            uniforms.type = static_cast<uint32_t>(l.type);
            uniforms.intensity = l.intensity;
            uniforms.quadratic = l.quadratic;
            uniforms.radius = l.radius;

            // TODO: Вычислить матрицу пространства источника (для потенциальной реализации теней)

            // Обновить параметры источника
            engine()->gfx()->update_light_uniforms(uniforms, ui.index);

            // Обновлено
            engine()->ecs()->remove_components<Dirty>(e);
        }
    }

    void System::update_light_dynamic_ubo() const
    {
        // Алиасы компонентов
        using Spatial   = scn::SpatialComponent;
        using Light     = scn::LightComponent;
        using UniformId = UniformIndexComponent;
        using State     = UniformStateComponent;

        // Пройти по всем сущностям с компонентами:
        // - Пространственные параметры
        // - Источник света
        // - Uniform index
        // - Состояние UBO
        for (auto [e, sp, l, ui, state] : engine()->ecs()->view<Spatial, Light, UniformId, State>())
        {
            if (!state.is_dirty) continue;

            // Параметры источника
            uniforms::LightSettings uniforms = {};
            uniforms.position  = {sp.position.x, sp.position.y, sp.position.z, 1.0f};
            uniforms.direction = {sp.rotation.x, sp.rotation.y, sp.rotation.z, 0.0f};
            uniforms.color     = l.color;
            uniforms.type      = static_cast<uint32_t>(l.type);
            uniforms.intensity = l.intensity;
            uniforms.quadratic = l.quadratic;
            uniforms.radius    = l.radius;

            // TODO: Вычислить матрицу пространства источника (для потенциальной реализации теней)

            // Обновить параметры источника
            engine()->gfx()->update_light_uniforms(uniforms, ui.index);

            // Обновлено
            state.is_dirty = false;
        }
    }

    void System::update_cam_ubo() const
    {
        // Алиасы компонентов
        using Spatial   = scn::SpatialComponent;
        using Camera    = scn::ViewComponent;
        using UniformId = UniformIndexComponent;
        using State     = UniformStateComponent;
        using Render    = RenderComponent;

        // Пройти по всем сущностям с компонентами:
        // - Пространственные параметры
        // - Камера
        // - Uniform index
        // - Состояние UBO
        // Где нет компонентов:
        // - Рендеринг
        for (auto [e, cam, sp, ui, state] : engine()->ecs()->view<
            Camera,
            Spatial,
            UniformId,
            State>(ecs::kMaskOf<Render>))
        {
            if (!state.is_dirty) continue;

            // Матрица поворота камеры
            glm::mat4 cam_rotation =
                glm::rotate(glm::mat4(1.0f), glm::radians(sp.rotation.y),glm::vec3(0.0f,1.0f,0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(sp.rotation.x),glm::vec3(1.0f,0.0f,0.0f));

            // Матрица смещения камеры
            glm::mat4 cam_translate = glm::translate(glm::mat4(1.0f), sp.position);

            // Итоговый UBO
            uniforms::Camera uniforms = {};
            uniforms.position = glm::vec4(sp.position, 1.0f);
            uniforms.view = glm::inverse(cam_translate * cam_rotation);
            uniforms.projection = glm::perspective(
                    glm::radians(cam.fov),
                    engine()->gfx()->renderer()->rendering_aspect(),
                    cam.near,
                    cam.far);

            // Обновить
            engine()->gfx()->update_cam_uniforms(uniforms, ui.index);
            state.is_dirty = false;
        }
    }

    void System::render_meshes() const
    {
        // Алиасы компонентов
        using Handles       = MeshHandlesComponent;
        using DirtyHandles  = DirtyHandlesComponent;
        using Render        = RenderComponent;
        using UniformId     = UniformIndexComponent;
        using Mesh          = scn::MeshComponent;

        // Пройти по всем сущностям с компонентами:
        // - Handles меша
        // - Uniform index
        // - Узел сцены "меш"
        // - Тег рендеринга
        // Где нет компонентов:
        // - Грязные handles
        for (auto[e, mh, ui, mesh, r_tag] : engine()->ecs()->view<
            Handles,
            UniformId,
            Mesh,
            Render>(ecs::kMaskOf<DirtyHandles>))
        {
            if (!mh.mesh){
                log_warn("Missing render handles for renderable mesh entity " + e.to_string());
            }

            if (mesh.materials.empty()){
                log_warn("Missing materials for renderable mesh entity" + e.to_string());
            }

            // Привязка всей геометрии меша
            subsystem()->renderer()->cmd_bind_geometry(mh.mesh, ui.index);

            // Проход по поверхностям
            for (uint32_t i = 0; i < mh.mesh.surfaces_count; ++i)
            {
                // Параметры поверхности
                const auto& surface = mh.mesh.surfaces[i];
                // Найти соответствующий экземпляр материала
                const auto& mat_e = mesh.materials[i];

                // Если меш еще не ссылался на entity материала - сослаться (запрос готовности)
                if (!mesh.materials_requested[i]){
                    mesh.materials_requested[i] = true;
                    ecs::inc_entity_refs(engine()->ecs(), mat_e);
                }

                // Если материал не готов - пропуск
                if (!engine()->ecs()->is_valid(mat_e)
                    || !engine()->ecs()->has<MaterialHandlesComponent>(mat_e)
                    || !engine()->ecs()->has<res::LoadedComponent>(mat_e)
                    || engine()->ecs()->has<DirtyHandles>(mat_e))
                {
                    continue;
                }

                // Получить UBO id и handles материала
                auto [mat_ubo, mat_hdl] = engine()->ecs()->get_components<
                    UniformId,
                    MaterialHandlesComponent>(mat_e);

                // Привязать материал, нарисовать поверхность
                subsystem()->renderer()->cmd_bind_material(mat_hdl.material, mat_ubo.index);
                subsystem()->renderer()->cmd_draw_geometry(surface.index_offset, surface.index_count);
            }
        }
    }
}
