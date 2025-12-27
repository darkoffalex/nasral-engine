#include "pch.h"
#include <nasral/res/system.h>
#include <nasral/engine.h>
#include <nasral/ecs/view.h>
#include <nasral/gfx/components.h>
#include <nasral/gfx/types.h>
#include <nasral/ecs/manager.h>
#include <nasral/res/resources/material.h>
#include <nasral/res/resources/texture.h>

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
        update_generic_resources(dt);
        update_texture_resources(dt);
    }

    void System::shutdown()
    {}

    void System::update_generic_resources([[maybe_unused]] float dt) const
    {
        using Desc = comp::Descriptor;
        using Req = comp::Request;
        using Rel = comp::Release;

        auto* ecs = engine()->ecs();
        auto* res = engine()->res();

        // Запросы обычных одиночных ресурсов
        for (auto[e, d, r] : ecs->view<Desc, Req>()){
            res->request(d.res_id, [this, entity = e](IResource* resource){
                on_generic_loaded(entity, resource);
            });

            ecs->remove_component_deferred<Req>(e);
        }

        // Освобождение обычных одиночных ресурсов
        for (auto[e, d, r] : ecs->view<Desc, Rel>()){
            res->release(d.res_id);
            ecs->remove_component_deferred<Rel>(e);
        }
    }

    void System::update_texture_resources([[maybe_unused]] float dt) const
    {
        using DescTexList = comp::DescriptorList<gfx::TextureType>;
        using Req = comp::Request;
        using Rel = comp::Release;

        auto* ecs = engine()->ecs();
        auto* res = engine()->res();

        // Запросы списка текстур
        for (auto[e, d, r] : ecs->view<DescTexList, Req>()){
            for (const gfx::TextureType tt : magic_enum::enum_values<gfx::TextureType>()){
                if (d.res_ids[tt] != kInvalidResourceId){
                    res->request(d.res_ids[tt], [this, entity = e, tt](IResource* resource){
                        this->on_texture_loaded(entity, resource, tt);
                    });
                }
            }

            ecs->remove_component_deferred<Req>(e);
        }

        // Освобождение списка текстур
        for (auto[e, d, r] : ecs->view<DescTexList, Rel>()){
            for (const gfx::TextureType tt : magic_enum::enum_values<gfx::TextureType>()){
                if (d.res_ids[tt] != kInvalidResourceId){
                    res->release(d.res_ids[tt]);
                }
            }

            ecs->remove_component_deferred<Rel>(e);
        }
    }

#pragma region handlers

    void System::on_generic_loaded(const ecs::EntityId& entity, IResource* res) const
    {
        auto* ecs = engine()->ecs();
        if (!ecs->is_valid(entity)){
            log_error("Trying to update invalid entity");
            return;
        }

        if (res->status() != Status::eLoaded){
            log_error("Trying to use resource that is not loaded");
            if (res->status() == Status::eError){
                ecs->add_component_deferred<comp::Error>(entity);
            }
            return;
        }

        // Для экземпляров материала
        if (ecs->has_component<gfx::comp::MaterialSettings>(entity))
        {
            // Тип ресурса должен соответствовать материалу
            const auto* m_res = dynamic_cast<Material*>(res);
            if (!res){
                log_error("Using wrong resource type for material instance update");
                return;
            }

            // Если компонент уже есть (мог быть добавлен при загрузке текстур) - задать значение
            if (ecs->has_component<gfx::comp::MaterialHandles>(entity)){
                auto& m_handles = ecs->get_component<gfx::comp::MaterialHandles>(entity);
                m_handles.material = m_res->render_handles();
            }
            // Если компонента нет - добавить новый
            else{
                gfx::comp::MaterialHandles m_handles;
                m_handles.material = m_res->render_handles();
                ecs->add_component<gfx::comp::MaterialHandles>(entity, m_handles);
            }
        }
    }

    void System::on_texture_loaded(const ecs::EntityId& entity, IResource* res, const gfx::TextureType type) const
    {
        auto* ecs = engine()->ecs();
        if (!ecs->is_valid(entity)){
            log_error("Trying to update invalid entity");
            return;
        }

        if (res->status() != Status::eLoaded){
            log_error("Trying to use resource that is not loaded");
            if (res->status() == Status::eError){
                ecs->add_component_deferred<comp::Error>(entity);
            }
            return;
        }

        // Для экземпляров материала
        if (ecs->has_component<gfx::comp::MaterialSettings>(entity))
        {
            // Тип ресурса должен соответствовать материалу
            const auto* t_res = dynamic_cast<Texture*>(res);
            if (!res){
                log_error("Using wrong resource type for material instance update");
                return;
            }

            // Если компонент уже есть (мог быть добавлен при загрузке материала) - задать значение
            if (ecs->has_component<gfx::comp::MaterialHandles>(entity)){
                auto& m_handles = ecs->get_component<gfx::comp::MaterialHandles>(entity);
                m_handles.textures[type] = t_res->render_handles();
            }
            // Если компонента нет - добавить новый
            else{
                gfx::comp::MaterialHandles m_handles;
                m_handles.textures[type] = t_res->render_handles();
                ecs->add_component_deferred<gfx::comp::MaterialHandles>(entity, m_handles);
            }
        }
    }

#pragma endregion

}
