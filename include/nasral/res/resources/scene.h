#pragma once

#include <nasral/res/resource.h>
#include <nasral/res/loader.h>
#include <nasral/scn/io/base_node.h>
#include <nasral/log/loggable.h>

namespace nasral::res
{
    class Manager;
    class Scene final : public IResource, public log::Loggable<Scene>
    {
    public:
        typedef std::unique_ptr<Scene> Ptr;

        struct Data
        {
            scn::io::Node::Ptr scene_root;
        };

        Scene(Manager* manager, ResourceId id, Loader<Data>::Ptr loader);
        ~Scene() override;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        [[nodiscard]] const scn::io::Node& scene_root() const {return *scene_root_;}

        void load() noexcept override;

    protected:
        Loader<Data>::Ptr loader_;
        scn::io::Node::Ptr scene_root_;
    };
}