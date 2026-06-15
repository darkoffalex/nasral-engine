#include "pch.h"
#include <nasral/scn/objects/mesh.h>
#include <nasral/gfx/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/ecs/utils.h>
#include <nasral/scn/manager.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Mesh::Mesh(Manager* manager, const NodeDesc& description)
        : Spatial(manager, description)
    {
        // Получить ресурс меша
        const auto fallback = engine()->res()->find(res::kBuiltinMeshCube).value_or(res::kInvalidResourceId);
        const auto mesh_res_id = engine()->res()->find(description.mesh.mesh_path).value_or(fallback);
        assert(mesh_res_id != res::kInvalidResourceId);

        // Ресурсы (ресурс геометрии меша)
        Components::Resources resources_c = {};
        resources_c.ids[0] = mesh_res_id;
        resources_c.active[0] = true;
        resources_c.statuses[0] = res::Status::eUnloaded;

        // Материалы (найти entities)
        Components::Mesh mesh_c = {};
        for (auto& m_uid : description.mesh.materials){
            const auto* mat = engine()->gfx()->find_material(m_uid);
            assert(mat != nullptr);
            if (mat == nullptr) continue;
            mesh_c.materials.push_back(mat->entity());
        }

        // Добавить необходимые компоненты
        engine()->ecs()->add_components_immediate<
            Components::Mesh,
            Components::Resources,
            Components::Handles,
            Components::UniformIndex,
            Components::DirtyHandles,
            Components::RenderTag
            >(entity(),
                {std::move(mesh_c.materials)},
                {resources_c.ids,resources_c.active,resources_c.statuses},
                {},
                {engine()->gfx()->object_ubo_ids().acquire()},
                {},
                {});

        // По динамическим объектам итерируемся всегда и проверяем не нужно ли пересчитать матрицы (dirty == true)
        // По статическим итерируемся лишь в том случае, если есть компонент DirtyUniform (редкие изменения)
        if (description.mesh.dynamic){
            engine()->ecs()->add_components<Components::UniformState>(entity(), {true});
        }else{
            engine()->ecs()->add_components<Components::DirtyUniform>(entity(), {});
        }
    }

    Mesh::~Mesh()
    {
        // Уменьшить кол-во ссылок на entity материалов
        auto& [materials, requested] = engine()->ecs()->get_component<Components::Mesh>(entity());
        for (size_t i = 0; i < materials.size(); ++i){
            if (!requested[i]) continue;
            if (!engine()->ecs()->is_valid(materials[i])) continue;
            ecs::dec_entity_refs(engine()->ecs(), materials[i]);
        }

        // Освобождение ресурсов
        engine()->ecs()->add_components<res::ReleaseComponent>(entity(), {});
    }

    data::NodeView Mesh::data_view() const
    {
        const auto [id, name, node, spatial, materials] = engine()->ecs()->get_components<
            Components::Uid,
            Components::Name,
            Components::Node,
            Components::Spatial,
            Components::Mesh
        >(entity());

        return data::MeshNodeView{
            {
                {id.id,name.name,node.type},
                spatial.position,
                spatial.rotation,
                spatial.scale,
            },
            materials.materials
        };
    }

    Node::Ptr Mesh::clone() const
    {
        const auto data = std::get<data::MeshNodeView>(Spatial::data_view());
        return Node::Ptr{new Mesh(subsystem(), {
            UniqueId::generate(),
            data.type,
            data.name,
            {data.position, data.rotation, data.scale},
            {},
            {},
            {is_dynamic(), material_uids(), mesh_path()}
        })};
    }

    bool Mesh::is_dynamic() const
    {
        return engine()->ecs()->has<Components::UniformState>(entity());
    }

    std::vector<UniqueId> Mesh::material_uids() const
    {
        std::vector<UniqueId> result;
        const auto [mesh] = engine()->ecs()->get_components<Components::Mesh>(entity());
        result.reserve(mesh.materials.size());

        for (auto& entity : mesh.materials){
            const auto* mat = engine()->gfx()->find_material(entity);
            assert(mat != nullptr);
            if (mat == nullptr) continue;
            result.push_back(mat->data_view().uid);
        }

        return result;
    }

    std::string Mesh::mesh_path() const
    {
        const auto [resources] = engine()->ecs()->get_components<Components::Resources>(entity());
        assert(resources.active[0] != false);
        assert(resources.ids[0] != res::kInvalidResourceId);
        return engine()->res()->path(resources.ids[0], false);
    }

    void Mesh::set_position(const glm::vec3& position) const
    {
        Spatial::set_position(position);
        invalidate_ubo();
    }

    void Mesh::set_rotation(const glm::vec3& rotation) const
    {
        Spatial::set_rotation(rotation);
        invalidate_ubo();
    }

    void Mesh::set_scale(const glm::vec3& scale) const
    {
        Spatial::set_scale(scale);
        invalidate_ubo();
    }

    void Mesh::set_material(const ecs::EntityId& material, const size_t index) const
    {
        // Компонент меша (материалы и статус запроса материала)
        auto& [materials, requested] = engine()->ecs()->get_component<Components::Mesh>(entity());
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

    void Mesh::set_mesh_resource(const res::ResourceId& resource_id) const
    {
        // Компонент ресурсов
        auto& [ids, active, statuses] = engine()->ecs()->get_component<Components::Resources>(entity());
        assert(active[0] != false);

        // Если нет изменений
        if (resource_id == ids[0]) return;

        // Если ресурс загружался и он валиден - освобождение
        if (statuses[0] != res::Status::eUnloaded &&
            ids[0] != res::kInvalidResourceId)
        {
            engine()->res()->release(ids[0]);
            statuses[0] = res::Status::eUnloaded;
        }

        // Задать новый и запросить
        ids[0] = resource_id;
        engine()->ecs()->add_components<
            res::RequestComponent,
            gfx::DirtyHandlesComponent>(entity(), {}, {});
    }

    void Mesh::invalidate_ubo() const
    {
        if (is_dynamic()){
            auto [ubo_state] = engine()->ecs()->get_components<Components::UniformState>(entity());
            ubo_state.is_dirty = true;
        }
        else if (!engine()->ecs()->has<Components::DirtyUniform>(entity())){
            engine()->ecs()->add_components<Components::DirtyUniform>(entity(), {});
        }
    }
}
