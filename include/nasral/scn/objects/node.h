#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/ecs/components.h>
#include <nasral/scn/components.h>

namespace nasral::scn
{
    class Manager;
    class Node : public SubsystemObject<Manager>, public log::Loggable<Node>
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<Node> Ptr;

        struct Components
        {
            using Uid = ecs::UidComponent;
            using Name = ecs::NameComponent;
            using Node = NodeComponent;
        };

        virtual ~Node();
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;

        [[nodiscard]] const ecs::EntityId& entity() const;
        [[nodiscard]] virtual data::NodeView data_view() const;
        [[nodiscard]] virtual std::string info() const;
        [[nodiscard]] virtual Ptr clone() const;

    protected:
        Node(Manager* manager, const NodeDesc& description);

    private:
        ecs::EntityId entity_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(scn::Node, "SCN")