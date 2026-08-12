#pragma once

#include <nasral/res/objects/resource.h>
#include <nasral/gfx/types.h>
#include <nasral/res/types.h>
#include <nasral/inp/types.h>

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
            std::vector<gfx::PostProcessingDesc> post_processes = {};
            std::string initial_scene = {};
            std::vector<inp::ActionDesc> action_bindings = {};
            float mouse_sensitivity = 0.0f;
        };

        ProjectFile(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~ProjectFile() override;

        ProjectFile(const ProjectFile&) = delete;
        ProjectFile& operator=(const ProjectFile&) = delete;

        [[nodiscard]] const auto& resources() const noexcept { return data_.resources; }
        [[nodiscard]] const auto& materials() const noexcept { return data_.materials; }
        [[nodiscard]] const auto& post_process_pipelines() const noexcept { return data_.post_processes; }
        [[nodiscard]] const auto& initial_scene() const noexcept { return data_.initial_scene; }
        [[nodiscard]] const auto& action_bindings() const noexcept { return data_.action_bindings; }
        [[nodiscard]] const auto& mouse_sensitivity() const noexcept { return data_.mouse_sensitivity; }

        void load() noexcept override;

    private:
        Data data_;
        Loader<Data>::Ptr loader_;
    };
}
