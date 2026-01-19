#include "pch.h"
#include <nasral/scn/io/base_node.h>
#include <nasral/ecs/entity.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/components.h>
#include <nasral/engine.h>

namespace nasral::scn::io
{
    Node::Node(Engine *engine, Data data)
        : IOStruct(engine)
        , io_node_data(std::move(data))
    {}

    void Node::unpack_to(const ecs::EntityId &entity_id, const UnpackFlags flags) const
    {
        if (!(flags & eUFSkipRootComp)){
            try_add_node_components(entity_id);
        }

        try_add_children_components(entity_id);
    }

    void Node::pack_from([[maybe_unused]] ecs::EntityId &entity_id)
    {
        // TODO: Implement
    }

    void Node::try_add_node_components(const ecs::EntityId& entity_id) const
    {
        auto* ecs = engine()->ecs();
        auto& node_c = ecs->get_or_add_component<comp::Node>(entity_id);
        node_c.uid = io_node_data.id;
        node_c.type = io_node_data.type;
    }

    void Node::try_add_children_components(const ecs::EntityId& entity_id, const bool unpack_children) const
    {
        if (io_node_data.children.empty()){
            return;
        }

        auto* ecs = engine()->ecs();
        auto& parent_cc = ecs->get_or_add_component<comp::NodeChildren>(entity_id);

        for (auto& child_ptr : io_node_data.children)
        {
            auto ce = ecs->spawn();
            auto& child_nc = ecs->get_or_add_component<comp::Node>(ce);

            parent_cc.children.push_back(ce);
            child_nc.parent = entity_id;

            if (unpack_children){
                child_ptr->unpack_to(ce, eUFStandard);
            }
        }
    }
}
