#include "pch.h"
#include <nasral/scn/entities/mesh.h>
#include <nasral/scn/entities/spatial.h>
#include <nasral/ecs/utils.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    MeshView MeshView::setup(ecs::Manager* ecs,
        const ecs::EntityId& id,
        const NodeDesc& desc)
    {
        auto* gfx = ecs->engine()->gfx();
        auto* res = ecs->engine()->res();

        // Подготовка ресурсов геометрии
        res::ResourcesComponent res_c = {};
        const auto fallback = res->find(res::kBuiltinMeshCube).value_or(res::kInvalidResourceId);
        constexpr auto res_index = static_cast<size_t>(ResIndices::eMesh);
        res_c.ids[res_index] = res->find(desc.mesh.mesh_path).value_or(fallback);
        res_c.active[res_index] = true;
        res_c.statuses[res_index] = res::Status::eUnloaded;

        // Подготовка материалов
        MeshComponent mesh_c = {};
        for (const auto& m_uid : desc.mesh.materials) {
            if (const auto* mat = gfx->find_material(m_uid)) {
                mesh_c.materials.push_back(mat->entity());
            }
        }

        // Выделение слота UBO
        gfx::UniformIndexComponent ubo_c{gfx->object_ubo_ids().acquire()};

        // Добавляем ровно свои компоненты одной операцией (без лишних перестановок в архетипах)
        if (desc.mesh.dynamic) {
            ecs->add_components_immediate(id,
                std::move(mesh_c),
                res_c,
                gfx::MeshHandlesComponent{},
                ubo_c,
                gfx::DirtyHandlesComponent{},
                gfx::RenderComponent{},
                gfx::UniformStateComponent{} // Динамический тег
            );
        } else {
            ecs->add_components_immediate(id,
                std::move(mesh_c),
                res_c,
                gfx::MeshHandlesComponent{},
                ubo_c,
                gfx::DirtyHandlesComponent{},
                gfx::RenderComponent{},
                gfx::DirtyUnformComponent{} // Статический тег
            );
        }

        return ecs->as_view<MeshView>(id).value();
    }

    const ecs::EntityId& MeshView::material(const size_t index) const
    {
        assert(index < get<MeshComponent>().materials.size() && "Invalid material index");
        return get<MeshComponent>().materials[index];
    }

    const res::ResourceId& MeshView::mesh_resource() const
    {
        return get<res::ResourcesComponent>().ids[static_cast<size_t>(ResIndices::eMesh)];
    }

    const res::Status& MeshView::mesh_resource_status() const
    {
        return get<res::ResourcesComponent>().statuses[static_cast<size_t>(ResIndices::eMesh)];
    }

    bool MeshView::is_dynamic() const
    {
        return subsystem()->has<gfx::UniformStateComponent>(id());
    }

    void MeshView::set_position(const glm::vec3& position) const{
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()){
            spatial->set_position(position);
            invalidate_ubo();
        }
    }

    void MeshView::set_rotation(const glm::vec3& rotation) const{
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()){
            spatial->set_rotation(rotation);
            invalidate_ubo();
        }
    }

    void MeshView::set_scale(const glm::vec3& scale) const{
        if (auto spatial = as_view<SpatialView>(); spatial.has_value()){
            spatial->set_scale(scale);
            invalidate_ubo();
        }
    }

    void MeshView::set_material(const ecs::EntityId& material, const size_t index) const
    {
        // Компонент меша (материалы и статус запроса материала)
        auto& [materials, requested] = engine()->ecs()->get_component<MeshComponent>(id());
        assert(index < materials.size());
        assert(engine()->ecs()->is_valid(material));

        // Уменьшить кол-во ссылок у предыдущего материала
        if (requested[index]){
            ecs::dec_entity_refs(engine()->ecs(), materials[index]);
            requested[index] = false;
        }

        // Задать новый материал
        materials[index] = material;
    }

    void MeshView::set_mesh_resource(const res::ResourceId& resource_id) const
    {
        // Индекс ресурса меша
        constexpr auto res_index = static_cast<size_t>(ResIndices::eMesh);

        // Компонент ресурсов
        auto& [ids, active, statuses] = engine()->ecs()->get_component<res::ResourcesComponent>(id());
        assert(active[res_index] != false);

        // Если нет изменений
        if (resource_id == ids[res_index]) return;

        // Если ресурс загружался и он валиден - освобождение
        if (statuses[res_index] != res::Status::eUnloaded &&
            ids[res_index] != res::kInvalidResourceId)
        {
            engine()->res()->release(ids[res_index]);
            statuses[res_index] = res::Status::eUnloaded;
        }

        // Задать новый и запросить
        ids[res_index] = resource_id;
        engine()->ecs()->add_components<
            res::RequestComponent,
            gfx::DirtyHandlesComponent>(id(), {}, {});
    }

    void MeshView::reset_materials() const
    {
        auto& [materials, requested] = engine()->ecs()->get_component<MeshComponent>(id());
        for (size_t i = 0; i < materials.size(); ++i){
            if (!requested[i]) continue;
            if (!engine()->ecs()->is_valid(materials[i])) continue;
            ecs::dec_entity_refs(engine()->ecs(), materials[i]);
        }
    }

    void MeshView::reset_mesh_resource() const
    {
        // Индекс ресурса меша
        constexpr auto res_index = static_cast<size_t>(ResIndices::eMesh);

        // Компонент ресурсов
        auto& [ids, active, statuses] = engine()->ecs()->get_component<res::ResourcesComponent>(id());
        assert(active[res_index] != false);

        // Если ресурс загружался и он валиден - освобождение
        if (statuses[res_index] != res::Status::eUnloaded &&
            ids[res_index] != res::kInvalidResourceId)
        {
            engine()->res()->release(ids[res_index]);
            statuses[res_index] = res::Status::eUnloaded;
        }

        // Инвалидировать handles
        engine()->ecs()->add_components<gfx::DirtyHandlesComponent>(id(), {});
    }

    void MeshView::invalidate_ubo() const
    {
        if (is_dynamic()){
            auto [ubo_state] = engine()->ecs()->get_components<gfx::UniformStateComponent>(id());
            ubo_state.is_dirty = true;
        }
        else if (!engine()->ecs()->has<gfx::DirtyUnformComponent>(id())){
            engine()->ecs()->add_components<gfx::DirtyUnformComponent>(id(), {});
        }
    }
}


