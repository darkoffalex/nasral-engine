#include "pch.h"
#include <nasral/gfx/objects/material.h>
#include <nasral/gfx/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    MaterialInstance::MaterialInstance(Manager* manager, const MaterialDesc& description)
        : SubsystemObject(manager)
        , entity_(ecs::EntityId::invalid())
    {
        // Активные ресурсы
        Components::Resources::IdsList resources_ids{};
        Components::Resources::ActiveList resources_active{};

        // Ресурс материала
        if (const auto mat_res_id = engine()->res()->find(description.base_material_path); mat_res_id.has_value()){
            resources_ids[eBaseMaterial] = mat_res_id.value();
            resources_active[eBaseMaterial] = true;
        }else{
            throw std::runtime_error("Material resource not found (" + description.base_material_path + ")");
        }

        // Ресурсы текстур
        for (const auto type : magic_enum::enum_values<TextureType>()){
            if (type == TextureType::TOTAL) continue;

            if (description.texture_paths[type].empty())
            {
                resources_ids[kTexResMap[type]] = engine()->res()->find_texture_fallback(type).value_or(res::kInvalidResourceId);
                resources_active[kTexResMap[type]] = true;
            }
            else
            {
                const auto tex_res_id = engine()->res()->find(description.texture_paths[type]);
                resources_ids[kTexResMap[type]] = tex_res_id.value_or(engine()->res()->find_texture_fallback(type).value_or(res::kInvalidResourceId));
                resources_active[kTexResMap[type]] = true;
            }
        }


        // Создать Entity
        entity_ = engine()->ecs()->spawn();

        // Настройки материала
        uniforms::Material settings = {};
        if (description.base_material_type == MaterialBaseType::ePhong){
            settings = uniforms::MaterialPhong{
                description.phong_settings.color,
                description.phong_settings.ambient,
                description.phong_settings.shininess,
                description.phong_settings.specular
            };
        }else if (description.base_material_type == MaterialBaseType::ePBR){
            settings = uniforms::MaterialPbr{
                description.pbr_settings.color,
                description.pbr_settings.roughness,
                description.pbr_settings.metallic,
                description.pbr_settings.ao,
                description.pbr_settings.emission
            };
        }

        // Добавить компоненты
        engine()->ecs()->add_components_immediate<
            Components::Uid,
            Components::Name,
            Components::Settings,
            Components::Handles,
            Components::UniformIndex,
            Components::UniformsDirty,
            Components::TextureDirty,
            Components::HandlesDirty,
            Components::Resources,
            Components::RefsCount>(entity_,
                {description.unique_id},
                {description.name},
                {description.base_material_type, settings, description.texture_samplers},
                {},
                {engine()->gfx()->material_ubo_ids().acquire()},
                {},
                {},
                {},
                {resources_ids, resources_active, {res::Status::eUnloaded}},
                {});

        // Информация о добавлении
        log_info("Material instance registered (" + info() + ")");
    }

    MaterialInstance::~MaterialInstance()
    {
        // Если есть загруженные ресурсы на момент уничтожения объекта:
        // - Добавить в список освобождаемых
        // - Добавить в список уничтожаемых
        if (engine()->ecs()->has_any<res::LoadedComponent, res::LoadingComponent>(entity_))
        {
            engine()->ecs()->add_components_immediate<res::ReleaseComponent, ecs::DestroyComponent>(entity_, {}, {});
        }
        // Если нет загруженных ресурсов на момент уничтожения:
        // - Добавить в список уничтожаемых
        else
        {
            engine()->ecs()->add_components_immediate<ecs::DestroyComponent>(entity_, {});
        }

        log_info("Material instance unregistered (" + info(false) + ")");
    }

    const ecs::EntityId& MaterialInstance::entity() const{
        return entity_;
    }

    MaterialInstance::Components::View MaterialInstance::data_view() const
    {
        const auto* ecs = subsystem()->engine()->ecs();
        const auto [id, name, settings, res, ubo_id] = ecs->get_components<
            Components::Uid,
            Components::Name,
            Components::Settings,
            Components::Resources,
            Components::UniformIndex
        >(entity_);

        return {
            id.id,
            name.name,
            settings.base_type,
            res.ids,
            settings.samplers,
            settings.uniforms,
            ubo_id.index
        };
    }

    std::string MaterialInstance::info(const bool full) const
    {
        const auto data = data_view();

        std::stringstream ss;
        ss << "UID: " << data.uid.to_string();

        if (full){
            ss  << ", Name: " << data.name
                << ", UBO index: " << data.uniform_index
                << ", Base type: " << magic_enum::enum_name(data.base_type)
                << ", Resource ID: " << data.resources[eBaseMaterial];
        }

        return ss.str();
    }

    void MaterialInstance::set_name(const std::string& name) const
    {
        const auto* ecs = subsystem()->engine()->ecs();
        auto& [name_c] = ecs->get_component<Components::Name>(entity_);
        name_c = name;
    }

    void MaterialInstance::set_uniforms(const uniforms::Material& uniforms) const
    {
        auto* ecs = subsystem()->engine()->ecs();
        auto& [bt, uniforms_c, samplers] = ecs->get_component<Components::Settings>(entity_);
        uniforms_c = uniforms;

        if (!ecs->has<Components::UniformsDirty>(entity_)){
            ecs->add_components<Components::UniformsDirty>(entity_, {});
        }
    }

    void MaterialInstance::set_texture_sampler(const TextureType type, const TextureSamplerType sampler_type) const
    {
        auto* ecs = subsystem()->engine()->ecs();
        auto& [bt, uniforms_c, samplers] = ecs->get_component<Components::Settings>(entity_);
        samplers[type] = sampler_type;

        if (!ecs->has<Components::TextureDirty>(entity_)){
            ecs->add_components<Components::TextureDirty>(entity_, {});
        }
    }
}
