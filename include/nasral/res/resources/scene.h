#pragma once

#include <vector>
#include <nasral/res/resource.h>
#include <nasral/res/loader.h>
#include <nasral/scn/types_io.h>
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
            std::vector<scn::io::Node> nodes;
        };

        Scene(Manager* manager, ResourceId id, Loader<Data>::Ptr loader);
        ~Scene() override;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        void load() noexcept override;

    protected:
        Loader<Data>::Ptr loader_;
        std::vector<scn::io::Node> nodes_;
    };
}