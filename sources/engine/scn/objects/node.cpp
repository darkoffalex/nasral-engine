#include "pch.h"
#include <nasral/scn/objects/node.h>
#include <nasral/gfx/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/manager.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Node::Node(Manager* manager, const NodeDesc& description)
        : SubsystemObject(manager)
        , entity_(ecs::EntityId::invalid())
    {
        entity_ = engine()->ecs()->spawn();

        engine()->ecs()->add_components_immediate<
            Components::Uid,
            Components::Name,
            Components::Node>(entity_,
                {description.unique_id},
                {description.name},
                {description.type});

        log_info("Scene node registered (" + Node::info() + ")");
    }

    Node::~Node(){
        auto* ecs = subsystem()->engine()->ecs();
        ecs->add_components<ecs::DestroyComponent>(entity_, {});
        log_info("Scene node unregistered (" + Node::info() + ")");
    }

    const ecs::EntityId& Node::entity() const{
        return entity_;
    }

    data::NodeView Node::data_view() const
    {
        const auto [id, name, node] = engine()->ecs()->get_components<
                Components::Uid,
                Components::Name,
                Components::Node
            >(entity_);

        return data::DummyNodeView{
            id.id,
            name.name,
            node.type
        };
    }

    std::string Node::info() const
    {
        const auto data = Node::data_view();
        auto [uid, name, type] = std::get<data::DummyNodeView>(data);

        std::stringstream ss;
        ss << "UID: " << uid.to_string();
        ss << ", Name: " << name;
        ss << ", Type: " << magic_enum::enum_name(type);
        return ss.str();
    }

    Node::Ptr Node::clone() const
    {
        const auto data = Node::data_view();
        auto [uid, name, type] = std::get<data::DummyNodeView>(data);

        return Ptr{new Node(subsystem(), {
            UniqueId::generate(),
            type,
            name
        })};
    }
}
