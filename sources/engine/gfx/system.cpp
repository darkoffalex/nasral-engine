#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/gfx/manager.h>
#include <nasral/gfx/objects/material.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/view.h>
#include <nasral/engine.h>
#include <nasral/res/components.h>
#include <nasral/scn/components.h>
#include <nasral/res/objects/material.h>
#include <nasral/res/objects/texture.h>
#include <nasral/res/objects/mesh.h>

namespace nasral::gfx
{
    System::System(Manager* m) : ecs::System<System, Manager>(m){}
    System::~System() = default;

    void System::on_init() const{
        log_info("ECS-system initialized");
    }

    void System::on_update([[maybe_unused]] const float delta) const{
        auto* ecs = subsystem()->engine()->ecs();
        auto* res = subsystem()->engine()->res();
        auto* gfx = subsystem();

        update_mtl_ubo(ecs, gfx);
        update_mtl_handles(ecs, res);
        update_mtl_textures(ecs, gfx);
    }

    void System::on_finalize() const{
        log_info("ECS-system finalized");
    }

    void System::update_mtl_ubo(ecs::Manager* ecs, const Manager* gfx)
    {
        // Алиасы компонентов
        using Settings  = MaterialSettingsComponent;
        using UniformId = UniformIndexComponent;
        using Dirty     = DirtyUnformComponent;

        // Пройти по всем сущностям с компонентами:
        // - Настройки материала
        // - Uniform index
        // - Грязный (не обновленный) uniform
        for (auto [e, ms, ui, du_tag] : ecs->view<Settings, UniformId, Dirty>())
        {
            // Для всех вариаций настроек материала
            std::visit([&, index = ui.index](auto&& uniforms){
                gfx->update_mat_uniforms(uniforms, index);
            }, ms.uniforms);

            // Обновлено
            ecs->remove_components<Dirty>(e);
        }
    }

    void System::update_mtl_handles(ecs::Manager* ecs, const res::Manager* res)
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
        for (auto [e, mh, rsc, d_tag, l_tag] : ecs->view<Handles, Resources, Dirty, Loaded>())
        {
            // Материал должен быть загружен
            if (kDebugBuild){
                assert(rsc.active[ResIndices::eBaseMaterial]);
                assert(rsc.ids[ResIndices::eBaseMaterial] != res::kInvalidResourceId);
            }

            // Ресурс материала (должен быть доступен)
            const auto* mat_res = res->get<res::Material>(rsc.ids[ResIndices::eBaseMaterial]);
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
                    const auto* tex_res = res->get<res::Texture>(rsc.ids[res_index]);
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
            ecs->remove_components<Dirty>(e);
        }
    }

    void System::update_mtl_textures(ecs::Manager* ecs, const Manager* gfx)
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
        for (auto [e, mh, ms, ui, dt_tag] : ecs->view<Handles, Settings, UniformId, DirtyTextures>(ecs::kMaskOf<DirtyHandles>))
        {
            // Итерация по типам текстур
            for (const auto type : magic_enum::enum_values<TextureType>()){
                if (type == TextureType::TOTAL) continue;

                gfx->update_mat_textures({
                    type,
                    ms.samplers[type],
                    mh.textures[type]
                }, ui.index);
            }

            // Обновлено
            ecs->remove_components<DirtyTextures>(e);
        }
    }

    void System::update_obj_static_ubo(ecs::Manager* ecs, const Manager* gfx)
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
        for (auto [e, sp, ui, r_tag, d_tag] : ecs->view<Spatial, UniformId, Render, Dirty>())
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
            gfx->update_obj_uniforms(uniforms, ui.index);

            // Обновлено
            ecs->remove_components<Dirty>(e);
        }
    }


    void System::update_obj_dynamic_ubo(ecs::Manager* ecs, const Manager* gfx)
    {
        // Алиасы компонентов
        using Spatial   = scn::SpatialComponent;
        using UniformId = UniformIndexComponent;
        using State     = UniformStateComponent;
        using Render    = RenderComponent;

        // Пройти по всем сущностям с компонентами:
        // - Handles объекта
        // - Пространственные параметры
        // - Uniform index
        // - Состояние UBO
        for (auto [e, sp, ui, state, r_tag] : ecs->view<Spatial, UniformId, State, Render>())
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
            gfx->update_obj_uniforms(uniforms, ui.index);

            // Обновлено
            state.is_dirty = false;
        }
    }

    void System::update_obj_mesh_handles(ecs::Manager* ecs, const res::Manager* res)
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
        for (auto [e, mh, rsc, d_tag, l_tag] : ecs->view<Handles, Resources, Dirty, Loaded>())
        {
            // Меш должен быть загружен
            if (kDebugBuild){
                assert(rsc.active[0]);
                assert(rsc.ids[0] != res::kInvalidResourceId);
            }

            // Ресурс материала (должен быть доступен)
            const auto* mesh_res = res->get<res::Mesh>(rsc.ids[0]);
            assert(mesh_res != nullptr && "Bad mesh resource");

            // Если загружен - обновить handles, если нет - fallback
            if (mesh_res->status() == res::Status::eLoaded){
                mh.mesh = mesh_res->render_handles();
            }else{
                // TODO: Fallback
            }

            // Обновлено
            ecs->remove_components<Dirty>(e);
        }
    }
}
