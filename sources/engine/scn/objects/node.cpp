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
        // Получить указатель на подсистему ECS
        auto* ecs = subsystem()->engine()->ecs();

        // Создать Entity
        entity_ = ecs->spawn();

        // Добавить компоненты
        ecs->add_components_immediate<
            Components::Uid,
            Components::Name,
            Components::Node>(entity_,
                {description.unique_id},
                {description.name},
                {description.type});

        // Информация о добавлении
        log_info("Scene node spawned (" + Node::info() + ")");
    }

    Node::~Node(){
        auto* ecs = subsystem()->engine()->ecs();
        ecs->add_components<ecs::DestroyComponent>(entity_, {});
    }

    const ecs::EntityId& Node::entity() const{
        return entity_;
    }

    data::NodeView Node::data_view() const
    {
        const auto* ecs = subsystem()->engine()->ecs();
        const auto& [id, name, node] = ecs->get_components<
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
