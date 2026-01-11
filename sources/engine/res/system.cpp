#include "pch.h"
#include <nasral/res/system.h>
#include <nasral/engine.h>
#include <nasral/ecs/view.h>
#include <nasral/gfx/components.h>
#include <nasral/gfx/types.h>
#include <nasral/ecs/manager.h>
#include <nasral/res/resources/material.h>
#include <nasral/res/resources/texture.h>
#include <nasral/res/resources/scene.h>

namespace nasral::res
{
    System::System(Engine* engine) : ecs::System<System>(engine)
    {}

    System::~System()
    = default;

    void System::init()
    {}

    void System::update([[maybe_unused]] const float dt)
    {
        update_requested();
        update_released();
    }

    void System::shutdown()
    {}

    bool System::validate(const ecs::EntityId& entity, const IResource* resource, const bool add_err) const
    {
        if (!engine()->ecs()->is_valid(entity)){
            log_error("Trying to update invalid entity");
            return false;
        }

        if (resource->status() != Status::eLoaded){
            log_error("Trying to use resource that is not loaded");
            if (resource->status() == Status::eError && add_err){
                engine()->ecs()->add_component_deferred<comp::Error>(entity);
            }
            return false;
        }

        return true;
    }

    void System::update_requested() const
    {
        using Desc = comp::Descriptor;
        using DescTexList = comp::DescriptorList<gfx::TextureType>;
        using Req = comp::Request;

        auto* ecs = engine()->ecs();
        auto* res = engine()->res();

        // Запросить нужные ресурсы
        for (auto[e, d, r] : ecs->view<Desc, Req>())
        {
            if (d.res_id != kInvalidResourceId)
            {
                res->request(d.res_id, [this, ecs, entity = e](IResource* resource)
                {
                    if (!validate(entity, resource)){
                        return;
                    }
                    if (ecs->has_component<gfx::comp::MaterialSettings>(entity)){
                        on_material_loaded(entity, resource);
                    }
                    else if (ecs->has_component<scn::comp::Mesh>(entity)){
                        on_mesh_loaded(entity, resource);
                    }
                    else if (ecs->has_component<scn::comp::Node>(entity)){
                        on_scene_loaded(entity, resource);
                    }
                });
            }

            // Запрос списка текстур (для материалов)
            if (ecs->has_component<DescTexList>(e))
            {
                auto& td = ecs->get_component<DescTexList>(e);
                for (const gfx::TextureType tt : magic_enum::enum_values<gfx::TextureType>())
                {
                    if (td.res_ids[tt] != kInvalidResourceId)
                    {
                        res->request(td.res_ids[tt], [this, entity = e, tt](IResource* resource)
                        {
                            if (!validate(entity, resource)){
                                return;
                            }

                            on_texture_loaded(entity, resource, tt);
                        });
                    }
                }
            }

            // Запрос выполнен
            ecs->remove_component_deferred<Req>(e);
        }
    }

    void System::update_released() const
    {
        using Desc = comp::Descriptor;
        using DescTexList = comp::DescriptorList<gfx::TextureType>;
        using Rel = comp::Release;

        auto* ecs = engine()->ecs();
        auto* res = engine()->res();

        // Освободить нужные ресурсы
        for (auto[e, d, r] : ecs->view<Desc, Rel>())
        {
            if (d.res_id != kInvalidResourceId){
                res->release(d.res_id);
            }

            if (ecs->has_component<DescTexList>(e)){
                auto& td = ecs->get_component<DescTexList>(e);
                for (const gfx::TextureType tt : magic_enum::enum_values<gfx::TextureType>()){
                    if (td.res_ids[tt] != kInvalidResourceId){
                        res->release(td.res_ids[tt]);
                    }
                }
            }

            // Освобождение выполнено
            ecs->remove_component_deferred<Rel>(e);
        }
    }

#pragma region handlers

    void System::on_texture_loaded(const ecs::EntityId& entity, IResource* res, const gfx::TextureType type) const
    {
        auto* ecs = engine()->ecs();

        // Тип ресурса должен соответствовать материалу
        const auto* t_res = dynamic_cast<Texture*>(res);
        if (!t_res){
            log_error("Using wrong resource type for material instance update");
            return;
        }

        // Задать компонент
        auto& m_handles = ecs->get_or_add_component<gfx::comp::MaterialHandles>(entity);
        m_handles.textures[type] = t_res->render_handles();
    }

    void System::on_material_loaded(const ecs::EntityId& entity, IResource* res) const
    {
        auto* ecs = engine()->ecs();

        // Тип ресурса должен соответствовать материалу
        const auto* m_res = dynamic_cast<Material*>(res);
        if (!m_res){
            log_error("Using wrong resource type for material instance update");
            return;
        }

        // Задать компонент
        auto& m_handles = ecs->get_or_add_component<gfx::comp::MaterialHandles>(entity);
        m_handles.material = m_res->render_handles();
    }

    void System::on_mesh_loaded(const ecs::EntityId& entity, IResource* res) const
    {
        (void)entity;
        (void)res;
        // TODO: Implement
    }

    void System::on_scene_loaded(const ecs::EntityId& entity, IResource* res) const
    {
        const auto* scn = engine()->scn();
        auto* ecs = engine()->ecs();

        const auto* s_res = dynamic_cast<Scene*>(res);
        if (!s_res){
            log_error("Using wrong resource type for scene node update");
            return;
        }

        core::IOStruct::UnpackFlags flags = core::IOStruct::eUFStandard;
        if (scn->root() == entity){
            flags |= core::IOStruct::eUFSkipRootComp;
        }

        // Распаковать узел в entity (создаст новые компоненты и вложенные узлы)
        s_res->scene_root().unpack_to(entity, flags);
        // Ресурс больше не нужен (можно выгрузить)
        ecs->add_component_deferred<comp::Release>(entity);
    }

#pragma endregion

}