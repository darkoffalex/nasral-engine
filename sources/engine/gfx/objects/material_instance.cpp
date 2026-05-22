#include "pch.h"
#include <nasral/gfx/objects/material_instance.h>
#include <nasral/gfx/renderer.h>
#include <nasral/ecs/manager.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    MaterialInstance::MaterialInstance(Renderer* renderer,
        const MaterialDesc& description,
        const UniqueId& id)
        : SubsystemObject(renderer)
        , entity_(ecs::EntityId::invalid())
    {
        // Получить указатели на все подсистемы
        auto* gfx = subsystem();
        auto* ecs = subsystem()->engine()->ecs();
        const auto* res = subsystem()->engine()->res();

        // ID ресурса материала
        const auto mat_res_id = res->find(description.material_resource);
        if (!mat_res_id.has_value()){
            throw std::runtime_error("Material resource not found");
        }

        // IDs ресурсов текстур
        ecs::ResourceListComponent<TextureType> tex_res_ids = {};
        for (const auto type : magic_enum::enum_values<TextureType>()){
            const auto& path = description.texture_paths[type];
            if (path.empty()){
                tex_res_ids.res_ids[type] = res->find_texture_fallback(type).value_or(res::kInvalidResourceId);
            }else{
                const auto res_id = res->find(path);
                if (!res_id.has_value()){
                    throw std::runtime_error("Texture resource not found");
                }
                tex_res_ids.res_ids[type] = res_id.value();
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
            Components::Handles,
            Components::UniformIndex,
            Components::UniformsDirty,
            Components::TextureDirty,
            Components::MaterialResource,
            Components::TextureResources>(entity_,
                {id},
                {description.name},
                {},
                {mat_ubo_id},
                {},
                {},
                {mat_res_id.value()},
                std::move(tex_res_ids));
    }

    MaterialInstance::~MaterialInstance()
    {
        // Отложить удаление entity
        auto* ecs = subsystem()->engine()->ecs();
        ecs->add_components<Components::PendingDestroy>(entity_, {});

        // Освобождение UBO ID
        auto* gfx = subsystem();
        const auto& [index] = ecs->get_component<Components::UniformIndex>(entity_);
        gfx->material_ubo_ids().release(index);
    }

    const UniqueId& MaterialInstance::uid() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [id] = ecs->get_component<Components::Uid>(entity_);
        return id;
    }

    const std::string& MaterialInstance::name() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [name] = ecs->get_component<Components::Name>(entity_);
        return name;
    }

    const MaterialBaseType& MaterialInstance::material_base_type() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [bt, uniforms, samplers] = ecs->get_component<Components::Settings>(entity_);
        return bt;
    }

    const res::ResourceId& MaterialInstance::material_resource() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [res_id] = ecs->get_component<Components::MaterialResource>(entity_);
        return res_id;
    }

    const EnumArray<TextureType, res::ResourceId>& MaterialInstance::texture_resources() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [res_ids] = ecs->get_component<Components::TextureResources>(entity_);
        return res_ids;
    }

    const EnumArray<TextureType, TextureSamplerType>& MaterialInstance::texture_samplers() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [bt, uniforms, samplers] = ecs->get_component<Components::Settings>(entity_);
        return samplers;
    }

    const uniforms::Material& MaterialInstance::uniforms() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [bt, uniforms, samplers] = ecs->get_component<Components::Settings>(entity_);
        return uniforms;
    }

    uint32_t MaterialInstance::uniform_index() const{
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [index] = ecs->get_component<Components::UniformIndex>(entity_);
        return index;
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
