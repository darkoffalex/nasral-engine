#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/gfx/manager.h>
#include <nasral/gfx/objects/material.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/view.h>
#include <nasral/engine.h>
#include <nasral/res/components.h>
#include <nasral/res/objects/material.h>
#include <nasral/res/objects/texture.h>

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

        process_materials_dirty_ubo(ecs, gfx);
        process_materials_dirty_handles(ecs, res);
        process_materials_dirty_textures(ecs, gfx);
    }

    void System::on_finalize() const{
        log_info("ECS-system finalized");
    }

    void System::process_materials_dirty_ubo(ecs::Manager* ecs, Manager* gfx)
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
            std::visit([&, index = ui.index](auto&& uniforms){
                gfx->update_mat_uniforms(uniforms, index);
            }, ms.uniforms);

            ecs->remove_components<Dirty>(e);
        }
    }

    void System::process_materials_dirty_handles(ecs::Manager* ecs, res::Manager* res)
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
        // - Грязные (не обновленне) handles
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

            ecs->remove_components<Dirty>(e);
        }
    }

    void System::process_materials_dirty_textures(ecs::Manager* ecs, Manager* gfx)
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
        // - Грязные (не обновленне) текстуры
        // Где нет компонентов:
        // - Грязные (не обновленне) handles
        for (auto [e, mh, ms, ui, dt_tag] : ecs->view<Handles, Settings, UniformId, DirtyTextures>(ecs::kMaskOf<DirtyHandles>))
        {
            for (const auto type : magic_enum::enum_values<TextureType>()){
                if (type == TextureType::TOTAL) continue;

                gfx->update_mat_textures({
                    type,
                    ms.samplers[type],
                    mh.textures[type]
                }, ui.index);
            }

            ecs->remove_components<DirtyTextures>(e);
        }
    }
}
