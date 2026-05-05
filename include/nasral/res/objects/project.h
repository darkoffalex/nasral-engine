#pragma once

#include <nasral/res/objects/resource.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>
#include <nasral/res/types.h>

namespace nasral::res
{
    class ProjectFile : public Resource
    {
    public:
        typedef std::unique_ptr<ProjectFile> Ptr;

        struct Data
        {
            std::vector<ResourceDesc> resources = {};
            std::vector<gfx::MaterialDesc> materials = {};
            std::string initial_scene = {};
            // TODO: Add action bindings here
            // TODO: Add sensitivity here
        };

        ProjectFile(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~ProjectFile() override;

        ProjectFile(const ProjectFile&) = delete;
        ProjectFile& operator=(const ProjectFile&) = delete;

        [[nodiscard]] const auto& resources() const noexcept { return data_.resources; }
        [[nodiscard]] const auto& materials() const noexcept { return data_.materials; }
        [[nodiscard]] const auto& initial_scene() const noexcept { return data_.initial_scene; }

        void load() noexcept override;

    private:
        Data data_;
        Loader<Data>::Ptr loader_;
    };
}
