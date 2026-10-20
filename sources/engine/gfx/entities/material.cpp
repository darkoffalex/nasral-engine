#include "pch.h"
#include <nasral/gfx/entities/material.h>
#include <nasral/ecs/utils.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    MaterialView MaterialView::setup(ecs::Manager *ecs,
        const ecs::EntityId &id,
        const MaterialDesc &desc)
    {
        auto* gfx = ecs->engine()->gfx();
        auto* res = ecs->engine()->res();

        // Активные ресурсы
        res::ResourcesComponent::IdsList resources_ids{};
        res::ResourcesComponent::ActiveList resources_active{};

        // Ресурс шейдерной программы
        if (const auto mat_res_id = res->find(desc.rasterization_shader_path); mat_res_id.has_value()){
            resources_ids[eRasterizationShader] = mat_res_id.value();
            resources_active[eRasterizationShader] = true;
        }else{
            throw std::runtime_error("Shader program resource not found (" + desc.rasterization_shader_path + ")");
        }

        // Ресурсы текстур
        for (const auto type : magic_enum::enum_values<TextureType>()){
            if (type == TextureType::TOTAL) continue;

            const auto fallback = res->find_texture_fallback(type).value_or(res::kInvalidResourceId);

            if (desc.texture_paths[type].empty())
            {
                resources_ids[kTexResMap[type]] = fallback;
                resources_active[kTexResMap[type]] = true;
            }
            else
            {
                resources_ids[kTexResMap[type]] = res->find(desc.texture_paths[type]).value_or(fallback);
                resources_active[kTexResMap[type]] = true;
            }
        }

        // Настройки материала
        uniforms::Material settings = {};
        if (desc.base_material_type == MaterialBaseType::ePhong){
            settings = uniforms::MaterialPhong{
                desc.phong_settings.color,
                desc.phong_settings.ambient,
                desc.phong_settings.shininess,
                desc.phong_settings.specular
            };
        }else if (desc.base_material_type == MaterialBaseType::ePBR){
            settings = uniforms::MaterialPbr{
                desc.pbr_settings.color,
                desc.pbr_settings.roughness,
                desc.pbr_settings.metallic,
                desc.pbr_settings.ao,
                desc.pbr_settings.emission
            };
        }

        // Добавить компоненты
        ecs->add_components_immediate<GFX_MATERIAL_COMPONENTS, GFX_MATERIAL_DIRTY_TAGS>(id,
            {desc.unique_id},
            {desc.name},
            {desc.base_material_type, settings, desc.texture_samplers},
            {},
            {gfx->material_ubo_ids().acquire()},
            {resources_ids, resources_active, {res::Status::eUnloaded}},
            {},
            {},
            {},
            {});

        return ecs->as_view<MaterialView>(id).value();
    }

    void MaterialView::set_name(const std::string &name) {
        get<ecs::NameComponent>().name = name;
    }

    void MaterialView::set_uniforms(const uniforms::Material &uniforms) {
        get<MaterialSettingsComponent>().uniforms = uniforms;
        invalidate_ubo();
    }

    void MaterialView::set_texture_sampler(const TextureType type, const TextureSamplerType sampler_type) {
        get<MaterialSettingsComponent>().samplers[type] = sampler_type;
        invalidate_textures();
    }

    void MaterialView::set_rasterization_shader_resource(const res::ResourceId &resource_id) const {
        // Индекс ресурса меша
        constexpr auto res_index = static_cast<size_t>(eRasterizationShader);
        // Установить ресурс
        set_resource(resource_id, res_index);
        invalidate_handles();
    }

    void MaterialView::set_texture_resource(const TextureType type, const res::ResourceId &resource_id) const {
        // Индекс ресурса текстуры
        const auto res_index = static_cast<size_t>(kTexResMap[type]);
        // Установить ресурс
        set_resource(resource_id, res_index);
        invalidate_handles();
        invalidate_textures();
    }

    void MaterialView::destroy(const bool immediate) const {
        if (immediate) {
            engine()->ecs()->add_components_immediate<ecs::DestroyComponent>(id(), {});
        }else {
            engine()->ecs()->add_components<ecs::DestroyComponent>(id(), {});
        }
    }

    void MaterialView::invalidate_ubo() const {
        engine()->ecs()->add_components<DirtyUnformComponent>(id(), {});
    }

    void MaterialView::invalidate_handles() const {
        engine()->ecs()->add_components<DirtyHandlesComponent>(id(), {});
    }

    void MaterialView::invalidate_textures() const {
        engine()->ecs()->add_components<DirtyTexturesComponent>(id(), {});
    }

    void MaterialView::set_resource(const res::ResourceId &resource_id, const size_t index) const
    {
        // Компонент ресурсов
        auto& [ids, active, statuses] = engine()->ecs()->get_component<res::ResourcesComponent>(id());
        assert(active[index] != false);

        // Если нет изменений
        if (resource_id == ids[index]) return;

        // Если ресурс загружался и он валиден - освобождение
        if (statuses[index] != res::Status::eUnloaded &&
            ids[index] != res::kInvalidResourceId)
        {
            engine()->res()->release(ids[index]);
            statuses[index] = res::Status::eUnloaded;
        }

        // Материал не загружен
        engine()->ecs()->remove_components<res::LoadedComponent>(id());

        // Задать новый и запросить
        ids[index] = resource_id;
        engine()->ecs()->add_components<res::RequestComponent>(id(), {});
    }
}
