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
        update_requested_materials();
        update_released_materials();
    }

    void System::shutdown()
    {}

    void System::update_requested_materials() const
    {
        using MatDesc = comp::MaterialDescriptors;
        using MatRequest = comp::MaterialRequest;

        // Сущности материалов с тегом MatRequest нуждаются в запросе ресурсов
        for (auto [e, d, r] : engine()->ecs()->view<MatDesc, MatRequest>())
        {
            // Запрос ресурса материала
            engine()->res()->request(d.mat_res_id, [this, entity = e](IResource* res){
                this->on_material_loaded(entity, res);
            });

            // Запрос ресурсов текстур (для тех, что указаны)
            for (const gfx::TextureType tt : magic_enum::enum_values<gfx::TextureType>()){
                if (d.tex_res_ids[tt] != kInvalidResourceId){
                    engine()->res()->request(d.tex_res_ids[tt], [this, entity = e, tt](IResource* res){
                        this->on_texture_loaded(entity, res, tt);
                    });
                }
            }

            // Убрать тег запроса (больше не нужно запрашивать)
            engine()->ecs()->remove_component_deferred<MatRequest>(e);
        }
    }

    void System::update_released_materials() const
    {
        using MatDesc = comp::MaterialDescriptors;
        using MatHandles = gfx::comp::MaterialHandles;
        using MatRelease = comp::MaterialRelease;

        // Сущности материалов с тегом MatRequest нуждаются в освобождении ресурса
        for (auto [e, d, h, r] : engine()->ecs()->view<MatDesc, MatHandles, MatRelease>())
        {
            // Если handle задан - освободить
            if (h.material){
                engine()->res()->release(d.mat_res_id);
            }

            // Если handle текстуры задан, и ResourceId корректен - освободить
            for (const gfx::TextureType tt : magic_enum::enum_values<gfx::TextureType>()){
                if (d.tex_res_ids[tt] != kInvalidResourceId && h.textures[tt]){
                    engine()->res()->release(d.tex_res_ids[tt]);
                }
            }

            // Убрать тег освобождения ресурса (больше не нужно освобождать)
            engine()->ecs()->remove_component_deferred<MatRelease>(e);
        }
    }

#pragma region handlers

    void System::on_material_loaded(const ecs::EntityId& entity, IResource* res) const
    {
        // Сущность должна быть валидна на момент готовности ресурса
        auto* ecs = engine()->ecs();
        if (!ecs->is_valid(entity)){
            log_error("Trying to change invalid material entity");
            return;
        }

        // Тип ресурса должен соответствовать материалу
        const auto* m_res = dynamic_cast<Material*>(res);
        if (!res){
            log_error("Wrong resource type");
            return;
        }

        // Если загрузка успешна (нет ошибки)
        using MatHandles = gfx::comp::MaterialHandles;
        if (res->status() == Status::eLoaded)
        {
            // Если компонент уже есть (мог быть добавлен при загрузке текстур) - задать значение
            if (ecs->has_component<MatHandles>(entity)){
                auto& m_handles = ecs->get_component<MatHandles>(entity);
                m_handles.material = m_res->render_handles();
            }
            // Если компонента нет - добавить новый
            else{
                MatHandles m_handles;
                m_handles.material = m_res->render_handles();
                ecs->add_component<MatHandles>(entity, m_handles);
            }
        }
        // Если ошибка загрузки
        else if (res->status() == Status::eError){
            // TODO: Обработать сценарий
        }
    }

    void System::on_texture_loaded(const ecs::EntityId& entity, IResource* res, const gfx::TextureType type) const
    {
        // Сущность должна быть валидна на момент готовности ресурса
        auto* ecs = engine()->ecs();
        if (!ecs->is_valid(entity)){
            log_error("Trying to change invalid material entity");
            return;
        }

        // Тип ресурса должен соответствовать текстуре
        const auto* t_res = dynamic_cast<Texture*>(res);
        if (!res){
            log_error("Wrong resource type");
            return;
        }

        // Если загрузка успешна (нет ошибки)
        using MatHandles = gfx::comp::MaterialHandles;
        if (res->status() == Status::eLoaded){
            // Если компонент уже есть (мог быть добавлен при загрузке материала) - задать значение
            if (ecs->has_component<MatHandles>(entity)){
                auto& m_handles = ecs->get_component<MatHandles>(entity);
                m_handles.textures[type] = t_res->render_handles();
            }
            // Если компонента нет - добавить новый
            else{
                MatHandles m_handles;
                m_handles.textures[type] = t_res->render_handles();
                ecs->add_component_deferred<MatHandles>(entity, m_handles);
            }
        }
        // Если ошибка загрузки
        else if (res->status() == Status::eError){
            // TODO: Обработать сценарий
        }
    }

#pragma endregion

}
