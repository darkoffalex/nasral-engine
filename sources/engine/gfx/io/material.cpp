#include "pch.h"
#include <nasral/engine.h>
#include <nasral/gfx/io/material.h>
#include <nasral/ecs/entity.h>
#include <nasral/ecs/manager.h>

namespace nasral::gfx::io
{
    Material::Material(Engine* engine, Data data)
        : IOStruct(engine)
        , io_material_data(std::move(data))
    {}

    void Material::unpack_to(const ecs::EntityId& entity_id, [[maybe_unused]] UnpackFlags flags) const
    {
        auto* ecs = engine()->ecs();
        auto* gfx = engine()->renderer();
        const auto& m = io_material_data;

        // Найти ID ресурса материала
        const auto m_res_id = engine()->res()->find(m.material_path);
        assert(m_res_id.has_value() && "Material resource not found in list");

        // Добавить необходимые компоненты сущности материала
        ecs->add_component<comp::MaterialSettings>(entity_id);
        ecs->add_component<comp::MaterialHandles>(entity_id);
        ecs->add_component<comp::DirtyUniform>(entity_id);
        ecs->add_component<comp::DirtyTextures>(entity_id);
        ecs->add_component<comp::UniformIndex>(entity_id);
        ecs->add_component<res::comp::AssetId>(entity_id);
        ecs->add_component<res::comp::Descriptor>(entity_id);
        ecs->add_component<res::comp::DescriptorList<TextureType>>(entity_id);

        // Получить основные компоненты
        auto& m_uid  = ecs->get_component<res::comp::AssetId>(entity_id);
        auto& m_desc = ecs->get_component<res::comp::Descriptor>(entity_id);
        auto& t_desc = ecs->get_component<res::comp::DescriptorList<TextureType>>(entity_id);
        auto& m_settings = ecs->get_component<comp::MaterialSettings>(entity_id);
        auto& m_ubo = ecs->get_component<comp::UniformIndex>(entity_id);


        // Задать UID
        m_uid.uid = m.id;
        // Задать дескриптор материала
        m_desc.res_id = m_res_id.value();
        // Задать дескрипторы текстур
        for (const TextureType tt : magic_enum::enum_values<TextureType>()){
            t_desc.res_ids[tt] = res::kInvalidResourceId;
            auto& tex_path = m.texture_paths[tt];
            if (!tex_path.empty()){
                auto tex_res_id = engine()->res()->find(tex_path);
                if (!tex_res_id.has_value()){
                    log_error("Texture resource not found in list: " + tex_path);
                    continue;
                }
                t_desc.res_ids[tt] = tex_res_id.value();
            }
        }

        // Индекс в UBO/SSBO
        // Внимание! Выделение ID материала (нужно затем освободить)
        m_ubo.index = gfx->material_ids().acquire();

        // Задать параметры материала
        m_settings.type = m.type;
        m_settings.uniforms = m.material_settings;
        m_settings.samplers = m.texture_samplers;
    }

    void Material::pack_from([[maybe_unused]] ecs::EntityId &entity_id)
    {}
}
