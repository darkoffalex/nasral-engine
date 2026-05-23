#include "pch.h"
#include <nasral/gfx/objects/material.h>
#include <nasral/gfx/renderer.h>
#include <nasral/ecs/manager.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    MaterialInstance::MaterialInstance(Renderer* renderer,
        const MaterialDesc& description)
        : SubsystemObject(renderer)
        , entity_(ecs::EntityId::invalid())
    {
        // Получить указатели на все подсистемы
        auto* gfx = subsystem();
        auto* ecs = subsystem()->engine()->ecs();
        const auto* res = subsystem()->engine()->res();

        // ID ресурса материала
        const auto mat_res_id = res->find(description.base_material_path);
        if (!mat_res_id.has_value()){
            throw std::runtime_error("Material resource not found");
        }

        // IDs ресурсов текстур
        EnumArray<TextureType, res::ResourceId> tex_res_ids = {};
        for (const auto type : magic_enum::enum_values<TextureType>()){
            const auto& path = description.texture_paths[type];
            if (path.empty()){
                tex_res_ids[type] = res->find_texture_fallback(type).value_or(res::kInvalidResourceId);
            }else{
                const auto res_id = res->find(path);
                if (!res_id.has_value()){
                    throw std::runtime_error("Texture resource not found");
                }
                tex_res_ids[type] = res_id.value();
            }
        }

        // Получить UBO ID для материала
        const auto mat_ubo_id = gfx->material_ubo_ids().acquire();

        // Создать Entity
        entity_ = ecs->spawn();

        // Добавить компоненты
        ecs->add_components_immediate<
            Components::Uid,
            Components::Name,
            Components::Settings,
            Components::Handles,
            Components::UniformIndex,
            Components::UniformsDirty,
            Components::TextureDirty,
            Components::MaterialResource,
            Components::TextureResources>(entity_,
                {description.unique_id},
                {description.name},
                {description.base_material_type, {}, description.texture_samplers},
                {},
                {mat_ubo_id},
                {},
                {},
                {mat_res_id.value()},
                {tex_res_ids});

        // Информация о добавлении
        log_info("Material instance registered (" + info_str() + ")");
    }

    MaterialInstance::~MaterialInstance()
    {
        // Отложить удаление entity
        auto* ecs = subsystem()->engine()->ecs();
        ecs->add_components_immediate<Components::PendingDestroy>(entity_, {});

        // Освобождение UBO ID
        auto* gfx = subsystem();
        const auto& [index] = ecs->get_component<Components::UniformIndex>(entity_);
        gfx->material_ubo_ids().release(index);

        // Информация об уничтожении
        log_info("Material instance unregistered (" + info_str(false) + ")");
    }

    const ecs::EntityId& MaterialInstance::entity() const{
        return entity_;
    }

    MaterialInstance::Components::View MaterialInstance::components() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [id, name, settings, res, tex, ubo_id] = ecs->get_components<
            Components::Uid,
            Components::Name,
            Components::Settings,
            Components::MaterialResource,
            Components::TextureResources,
            Components::UniformIndex
        >(entity_);

        return {
            id.id,
            name.name,
            settings.base_type,
            res.res_id,
            tex.res_ids,
            settings.samplers,
            settings.uniforms,
            ubo_id.index
        };
    }

    std::string MaterialInstance::info_str(const bool full) const{
        const auto data = components();

        std::stringstream ss;
        ss << "UID: " << data.uid.to_string();

        if (full){
            ss  << ", Name: " << data.name
                << ", UBO index: " << data.uniform_index
                << ", Base type: " << magic_enum::enum_name(data.base_type)
                << ", Resource ID: " << data.material_resource;
        }

        return ss.str();
    }

    void MaterialInstance::set_name(const std::string& name) const{
        const auto* ecs = subsystem()->engine()->ecs();
        auto& [name_c] = ecs->get_component<Components::Name>(entity_);
        name_c = name;
    }

    void MaterialInstance::set_uniforms(const uniforms::Material& uniforms) const{
        auto* ecs = subsystem()->engine()->ecs();
        auto& [bt, uniforms_c, samplers] = ecs->get_component<Components::Settings>(entity_);
        uniforms_c = uniforms;

        if (!ecs->has_components<Components::UniformsDirty>(entity_)){
            ecs->add_components_immediate<Components::UniformsDirty>(entity_, {});
        }
    }

    void MaterialInstance::set_texture_resource(const TextureType type, const res::ResourceId id) const{
        auto* ecs = subsystem()->engine()->ecs();
        auto& [res_ids] = ecs->get_component<Components::TextureResources>(entity_);
        res_ids[type] = id;

        if (!ecs->has_components<Components::TextureDirty>(entity_)){
            ecs->add_components_immediate<Components::TextureDirty>(entity_, {});
        }
    }

    void MaterialInstance::set_texture_sampler(const TextureType type, const TextureSamplerType sampler_type) const{
        auto* ecs = subsystem()->engine()->ecs();
        auto& [bt, uniforms_c, samplers] = ecs->get_component<Components::Settings>(entity_);
        samplers[type] = sampler_type;

        if (!ecs->has_components<Components::TextureDirty>(entity_)){
            ecs->add_components_immediate<Components::TextureDirty>(entity_, {});
        }
    }
}
