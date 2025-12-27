#pragma once

#include <vector>
#include <nasral/res/resource.h>
#include <nasral/res/loader.h>
#include <nasral/gfx/types_io.h>
#include <nasral/log/loggable.h>

namespace nasral::res
{
    class Manager;
    class Project final : public IResource, public log::Loggable<Project>
    {
    public:
        typedef std::unique_ptr<Project> Ptr;

        struct Data
        {
            std::vector<gfx::io::Material> materials;
            std::string initial_scene_path;
        };

        Project(Manager* manager, ResourceId id, Loader<Data>::Ptr loader);
        ~Project() override;

        Project(const Project&) = delete;
        Project& operator=(const Project&) = delete;

        [[nodiscard]] const std::vector<gfx::io::Material>& materials() const {return materials_;}
        [[nodiscard]] ResourceId initial_scene() const {return initial_scene_;}

        void load() noexcept override;

    protected:
        Loader<Data>::Ptr loader_;
        std::vector<gfx::io::Material> materials_;
        ResourceId initial_scene_;
    };
}