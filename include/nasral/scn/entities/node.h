#pragma once

#include <nasral/ecs/entity_view.h>
#include <nasral/scn/components.h>

#define SCN_NODE_COMPONENTS \
    ecs::UidComponent, \
    ecs::NameComponent, \
    scn::NodeComponent

namespace nasral::scn
{
    class NodeView : public ecs::EntityView<SCN_NODE_COMPONENTS>
    {
    public:
        using EntityView::EntityView;

        static NodeView setup(ecs::Manager* ecs
            , const ecs::EntityId& id
            , const NodeDesc& desc);

        [[nodiscard]] const UniqueId& uid();
        [[nodiscard]] const std::string& name();
        [[nodiscard]] const NodeType& type();

        void set_uid(const UniqueId& uid);
        void set_name(const std::string& name);
        void set_type(const NodeType& type);
    };

    template<>
    struct NodeSpawner<NodeView>
    {
        static std::optional<NodeView> spawn(ecs::Manager* ecs, const NodeDesc& desc)
        {
            const auto entity = ecs->spawn();
            return NodeView::setup(ecs, entity, desc);
        }
    };
}