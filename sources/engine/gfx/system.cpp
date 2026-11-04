#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/gfx/manager.h>
#include <nasral/gfx/entities/material.h>
#include <nasral/gfx/entities/screen_pass.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/view.h>
#include <nasral/ecs/utils.h>
#include <nasral/res/components.h>
#include <nasral/scn/components.h>
#include <nasral/res/objects/texture.h>
#include <nasral/res/objects/mesh.h>
#include <nasral/res/objects/shader_program.h>
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

        // Экранные проходы
        update_sp_ubo();
        update_sp_handles();
        update_sp_destroy();
        update_sp_states();

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

    void System::on_finalize() const {
        update_mtl_destroy();
        update_sp_destroy();
        log_info("ECS-system finalized");
    }

    void System::on_render() const
    {
        // Проход растеризации
        engine()->gfx()->renderer()->cmd_begin_rasterization_pass();
        render_meshes();
        engine()->gfx()->renderer()->cmd_end_rasterization_pass();

        // Цепочка экранных проходов (пост-обработка)
        bool post_processed = false;
        auto pass_e = engine()->gfx()->find_first_screen_pass();
        while (pass_e != ecs::EntityId::invalid()) {
            const auto pass_view = engine()->ecs()->as_view<ScreenPassView>(pass_e);

            if (!pass_view->is_ready()) {
                if (!pass_view->is_resources_requested()) pass_view->request_resources();
                post_processed = false;
                break;
            }

            pass_e = pass_view->next_pass();
            engine()->gfx()->renderer()->cmd_begin_screen_pass(pass_view->input_attachments(), pass_view->output_attachments());
            engine()->gfx()->renderer()->cmd_bind_shader_program(pass_view->shader_program());
            engine()->gfx()->renderer()->cmd_draw_screen_quad(pass_view->uniform_index());
            engine()->gfx()->renderer()->cmd_end_screen_pass();

            post_processed = true;
        }

        // Отправка вложения на показ
        engine()->gfx()->renderer()->cmd_blit_to_screen(post_processed ? FrameAttachmentType::eCompositing : FrameAttachmentType::eColor);
    }

    void System::update_mtl_ubo() const {
        // Алиасы компонентов
        using Settings  = MaterialComponent;
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

    void System::update_mtl_handles() const {
        // Алиасы компонентов
        using ShaderHandles  = ShaderHandlesComponent;
        using TextureHandles = TextureHandlesComponent;
        using Resources      = res::ResourcesComponent;
        using Dirty          = DirtyHandlesComponent;
        using Loaded         = res::LoadedComponent;

        // Алиасы для индексов ресурсов
        using ResIndices = MaterialView::ResIndices;

        // Пройти по всем сущностям с компонентами:
        // - Handles шейдерной программы
        // - Handles текстур
        // - Список ресурсов
        // - Грязные (не обновленные) handles
        // - Ресурсы загружены
        // Где нет компонентов:
        // - Экранный проход
        for (auto&& [e, sh, th, rsc, d_tag, l_tag] : engine()->ecs()->view<
            ShaderHandles,
            TextureHandles,
            Resources,
            Dirty,
            Loaded>(ecs::kMaskOf<ScreenPassComponent>))
        {
            // Шейдер растеризации должен быть загружен
            if (kDebugBuild){
                assert(rsc.active[ResIndices::eRasterizationShader]);
                assert(rsc.ids[ResIndices::eRasterizationShader] != res::kInvalidResourceId);
            }

            // Ресурс шейдера должен существовать
            const auto* shader_res = engine()->res()->get<res::ShaderProgram>(rsc.ids[ResIndices::eRasterizationShader]);
            assert(shader_res != nullptr && "Bad shader");

            // Если загружен - обновить handles, если нет - fallback
            if (shader_res->status() == res::Status::eLoaded){
                sh.shader_program = shader_res->render_handles();
            }else{
                // TODO: Fallback
            }

            // Итерация по типам текстур
            for (const auto type : magic_enum::enum_values<TextureType>())
            {
                if (type == TextureType::TOTAL) continue;

                // Если текстура используется
                if (const auto res_index = MaterialView::kTexResMap[type]; rsc.active[res_index])
                {
                    // Текстура должна быть задана
                    assert(rsc.ids[res_index] != res::kInvalidResourceId);

                    // Ресурс должен существовать
                    const auto* tex_res = engine()->res()->get<res::Texture>(rsc.ids[res_index]);
                    assert(tex_res != nullptr && "Bad texture");

                    if (tex_res->status() == res::Status::eLoaded){
                        th.textures[type] = tex_res->render_handles();
                    }
                    else{
                        // TODO: Fallback
                    }
                }
                else{
                    th.textures[type] = {};
                }
            }

            // Обновлено
            engine()->ecs()->remove_components<Dirty>(e);
        }
    }

    void System::update_mtl_textures() const
    {
        // Алиасы компонентов
        using Handles       = TextureHandlesComponent;
        using Material      = MaterialComponent;
        using UniformId     = UniformIndexComponent;
        using DirtyHandles  = DirtyHandlesComponent;
        using DirtyTextures = DirtyTexturesComponent;

        // Пройти по всем сущностям с компонентами:
        // - Handles текстур
        // - Настройки материала
        // - Uniform index
        // - Грязные (не обновленные) текстуры
        // Где нет компонентов:
        // - Грязные (не обновленные) handles
        for (auto [e, th, ms, ui, dt_tag] : engine()->ecs()->view<
            Handles,
            Material,
            UniformId,
            DirtyTextures>(ecs::kMaskOf<DirtyHandles>))
        {
            // Итерация по типам текстур
            for (const auto type : magic_enum::enum_values<TextureType>()){
                if (type == TextureType::TOTAL) continue;
                if (!th.textures[type]) continue;

                engine()->gfx()->update_mat_textures({
                    type,
                    ms.samplers[type],
                    th.textures[type]
                }, ui.index);
            }

            // Обновлено
            engine()->ecs()->remove_components<DirtyTextures>(e);
        }
    }

    void System::update_mtl_destroy() const
    {
        // Алиасы компонентов
        using Material  = MaterialComponent;
        using UniformId = UniformIndexComponent;
        using Destroy   = ecs::DestroyComponent;

        // Пройти по всем сущностям с компонентами
        // - Параметры материала
        // - Uniform index
        // - Уничтожение
        // Внимание: ожидается, что в конце полной итерации update сущность удаляется (что предотвратит повторную обработку)
        for (const auto& [e, ms, ui, d_tag] : engine()->ecs()->view<Material, UniformId, Destroy>())
        {
            engine()->gfx()->material_ubo_ids().release(ui.index);
        }
    }

    void System::update_sp_ubo() const {
        // Алиасы компонентов
        using Settings  = ScreenPassComponent;
        using UniformId = UniformIndexComponent;
        using Dirty     = DirtyUnformComponent;

        // Пройти по всем сущностям с компонентами:
        // - Настройки материала
        // - Uniform index
        // - Грязный (не обновленный) uniform
        for (auto [e, sps, ui, du_tag] : engine()->ecs()->view<Settings, UniformId, Dirty>())
        {
            // Для всех вариаций настроек материала
            std::visit([&, index = ui.index](auto&& uniforms){
                engine()->gfx()->update_screen_pass_uniforms(uniforms, index);
            }, sps.uniforms);

            // Обновлено
            engine()->ecs()->remove_components<Dirty>(e);
        }
    }

    void System::update_sp_handles() const {
        // Алиасы компонентов
        using ShaderHandles  = ShaderHandlesComponent;
        using Resources      = res::ResourcesComponent;
        using Dirty          = DirtyHandlesComponent;
        using Loaded         = res::LoadedComponent;

        // Алиасы для индексов ресурсов
        using ResIndices = ScreenPassView::ResIndices;

        // Пройти по всем сущностям с компонентами:
        // - Handles шейдерной программы
        // - Handles текстур
        // - Список ресурсов
        // - Грязные (не обновленные) handles
        // - Ресурсы загружены
        // Где нет компонентов:
        // - Материал
        for (auto&& [e, sh, rsc, d_tag, l_tag] : engine()->ecs()->view<
            ShaderHandles,
            Resources,
            Dirty,
            Loaded>(ecs::kMaskOf<MaterialComponent>))
        {
            // Шейдер растеризации должен быть загружен
            if (kDebugBuild){
                assert(rsc.active[ResIndices::eScreenPassShader]);
                assert(rsc.ids[ResIndices::eScreenPassShader] != res::kInvalidResourceId);
            }

            // Ресурс шейдера должен существовать
            const auto* shader_res = engine()->res()->get<res::ShaderProgram>(rsc.ids[ResIndices::eScreenPassShader]);
            assert(shader_res != nullptr && "Bad shader");

            // Если загружен - обновить handles, если нет - fallback
            if (shader_res->status() == res::Status::eLoaded){
                sh.shader_program = shader_res->render_handles();
            }else{
                // TODO: Fallback
            }

            // Обновлено
            engine()->ecs()->remove_components<Dirty>(e);
        }
    }

    void System::update_sp_destroy() const {
        // Алиасы компонентов
        using ScreenPass = ScreenPassComponent;
        using UniformId  = UniformIndexComponent;
        using Destroy    = ecs::DestroyComponent;

        // Пройти по всем сущностям с компонентами
        // - Параметры экранного прохода
        // - Uniform index
        // - Уничтожение
        // Внимание: ожидается, что в конце полной итерации update сущность удаляется (что предотвратит повторную обработку)
        for (const auto& [e, ms, ui, d_tag] : engine()->ecs()->view<ScreenPass, UniformId, Destroy>())
        {
            // Освободить uniform buffer object
            engine()->gfx()->screen_pass_ubo_ids().release(ui.index);

            // Пометить entity для освобождения ресурсов
            engine()->ecs()->add_components<res::ReleaseComponent>(e, {});
        }
    }

    void System::update_sp_states() const {
        // Алиасы компонентов
        using ScreenPass = ScreenPassComponent;
        using DirtyTag   = ecs::DirtyStateComponent;

        // Пройти по всем сущностям с компонентами
        // - Параметры экранного прохода
        // - Грязная entity (требует обновления состояния)
        for (const auto& [e, ms, d_tag] : engine()->ecs()->view<ScreenPass, DirtyTag>()) {
            // Получить соответствующий entity ID для предыдущего экранного прохода
            if (engine()->ecs()->has<ecs::PrevComponent>(e)) {
                auto&[uid, entity] = engine()->ecs()->get_component<ecs::PrevComponent>(e);
                entity = engine()->gfx()->find_screen_pass(uid);
            }
            // Получить соответствующий entity ID для следующего экранного прохода
            if (engine()->ecs()->has<ecs::NextComponent>(e)) {
                auto&[uid, entity] = engine()->ecs()->get_component<ecs::NextComponent>(e);
                entity = engine()->gfx()->find_screen_pass(uid);
            }

            // Обновлено
            engine()->ecs()->remove_components<DirtyTag>(e);
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
        for (const auto& [e, sp, ui, r_tag, d_tag] : engine()->ecs()->view<Spatial, UniformId, Render, Dirty>())
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
        for (auto&& [e, sp, ui, state, r_tag] : engine()->ecs()->view<Spatial, UniformId, State, Render>())
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
        for (auto&& [e, mh, rsc, d_tag, l_tag] : engine()->ecs()->view<Handles, Resources, Dirty, Loaded>())
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
        for (const auto& [e, sp, l, ui, d_tag] : engine()->ecs()->view<Spatial, Light, UniformId, Dirty>())
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
        for (auto&& [e, sp, l, ui, state] : engine()->ecs()->view<Spatial, Light, UniformId, State>())
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
        for (auto&& [e, cam, sp, ui, state] : engine()->ecs()->view<
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
            uniforms.view_inverse = cam_translate * cam_rotation;
            uniforms.view = glm::inverse(uniforms.view_inverse);
            uniforms.projection = glm::perspectiveRH_ZO(
                    glm::radians(cam.fov),
                    engine()->gfx()->renderer()->aspect_ratio(),
                    cam.near,
                    cam.far);
            uniforms.projection_inverse = glm::inverse(uniforms.projection);

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
        for (auto&& [e, mh, ui, mesh, r_tag] : engine()->ecs()->view<
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

            // Привязка геометрии меша
            subsystem()->renderer()->cmd_bind_mesh_geometry(mh.mesh);

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
                const auto mat_view = engine()->ecs()->as_view<MaterialView>(mat_e);
                if (!mat_view->is_ready()) {
                    continue;
                }

                // Привязать шейдерную программу
                subsystem()->renderer()->cmd_bind_shader_program(mat_view->shader_program());
                subsystem()->renderer()->cmd_draw_bound_geometry(
                    surface.index_offset,
                    surface.index_count,
                    ui.index,
                    mat_view->uniform_index());
            }
        }
    }
}
