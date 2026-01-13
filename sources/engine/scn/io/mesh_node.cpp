#include "pch.h"
#include <nasral/scn/io/mesh_node.h>
#include <nasral/ecs/entity.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/components.h>
#include <nasral/engine.h>

namespace nasral::scn::io
{
    MeshNode::MeshNode(Engine *engine, std::tuple<Node::Data, SpatialNode::Data, Data> data_tuple)
        : SpatialNode(engine, {std::move(std::get<0>(data_tuple)), std::get<1>(data_tuple)})
        , io_mesh_data(std::get<2>(data_tuple))
    {}

    void MeshNode::unpack_to([[maybe_unused]] const ecs::EntityId &entity_id, [[maybe_unused]] UnpackFlags flags) const
    {
        try_add_node_components(entity_id);
        try_add_spatial_components(entity_id, true);

        if (!try_add_mesh_components(entity_id, true)){
            engine()->logger()->error("Failed to add mesh components: " + io_mesh_data.mesh_path);
        }

        if (!io_node_data.children.empty()){
            engine()->logger()->warn("Children are not supported for MeshNode!");
            assert(false && "Children are not supported for MeshNode!");
        }
    }

    void MeshNode::pack_from([[maybe_unused]] ecs::EntityId &entity_id)
    {
        // TODO: Implement
    }

    bool MeshNode::try_add_mesh_components(const ecs::EntityId& entity_id, const bool request_resource) const
    {
        auto* log = engine()->logger();
        auto* ecs = engine()->ecs();
        const auto* gfx = engine()->renderer();
        const auto* res = engine()->res();

        const auto mesh_rh = res->find(io_mesh_data.mesh_path);
        if (!mesh_rh.has_value()){
            log->error("Mesh resource not found in list: " + io_mesh_data.mesh_path);
            return false;
        }

        const auto material_e = gfx->find_material_entity(io_mesh_data.material_id);
        if (!material_e.has_value()){
            log->error("Material entity not found for mesh: " + io_mesh_data.mesh_path);
            return false;
        }

        ecs->add_component<comp::Mesh>(entity_id);
        ecs->add_component<res::comp::Descriptor>(entity_id);

        auto& mesh_c = ecs->get_component<comp::Mesh>(entity_id);
        auto& res_c = ecs->get_component<res::comp::Descriptor>(entity_id);

        mesh_c.material_entity = material_e.value();
        res_c.res_id = mesh_rh.value();

        if (request_resource){
            ecs->add_component<res::comp::Request>(entity_id);
        }

        return true;
    }
}
