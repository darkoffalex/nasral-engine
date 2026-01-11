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
        auto* ecs = engine()->ecs();

        // Если не нужно пропускать обновление корня
        if (!(flags & eUFSkipRootComp)){
            auto& node = ecs->get_or_add_component<comp::Node>(entity_id);
            node.uid = io_node_data.id;
        }

        // Если нет потомков - выход
        if (io_node_data.children.empty()){
            return;
        }

        // Добавить потомков
        ecs->add_component<comp::NodeChildren>(entity_id);
        for (auto& child_ptr : io_node_data.children)
        {
            auto child_entity = ecs->spawn();
            auto& parent_cc = ecs->get_component<comp::NodeChildren>(entity_id);
            auto& child_nc = ecs->get_or_add_component<comp::Node>(child_entity);

            parent_cc.children.push_back(child_entity);
            child_nc.parent = entity_id;
            child_ptr->unpack_to(child_entity, eUFStandard);
        }
    }

    void Node::pack_from([[maybe_unused]] ecs::EntityId &entity_id)
    {
    }
}
