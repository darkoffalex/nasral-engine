#pragma once

#include <nasral/scn/objects/node.h>
#include <nasral/gfx/components.h>

namespace nasral::scn
{
    class Manager;
    class Spatial : public Node
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<Node> Ptr;

        struct Components : Node::Components
        {
            using Spatial = SpatialComponent;
        };

        ~Spatial() override;
        Spatial(const Spatial&) = delete;
        Spatial& operator=(const Spatial&) = delete;

        [[nodiscard]] data::NodeView data_view() const override;
        [[nodiscard]] Node::Ptr clone() const override;
        virtual void set_position(const glm::vec3& position) const;
        virtual void set_rotation(const glm::vec3& rotation) const;
        virtual void set_scale(const glm::vec3& scale) const;

    protected:
        Spatial(Manager* manager, const NodeDesc& description);
    };
}