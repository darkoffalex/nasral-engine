#pragma once

#include <nasral/res/objects/resource.h>
#include <nasral/scn/types.h>

namespace nasral::res
{
    class Scene final : public Resource
    {
    public:
        typedef std::unique_ptr<Scene> Ptr;

        struct Data
        {
            std::vector<scn::NodeDesc> nodes = {};
            scn::ScreenFxStateDesc screen_fx = {};
        };

        Scene(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~Scene() override;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        [[nodiscard]] const auto& nodes() const noexcept { return data_.nodes; }
        [[nodiscard]] const auto& screen_fx() const noexcept { return data_.screen_fx; }

        void load() noexcept override;
    private:
        Data data_;
        Loader<Data>::Ptr loader_;
    };
}