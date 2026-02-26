#pragma once

#include <vector>
#include <nasral/res/resource.h>
#include <nasral/res/loader.h>
#include <nasral/gfx/io/material.h>
#include <nasral/log/loggable.h>
#include <nasral/inp/types.h>

namespace nasral::res
{
    class Manager;
    class Project final : public IResource, public log::Loggable<Project>
    {
    public:
        typedef std::unique_ptr<Project> Ptr;
        typedef std::pair<std::string, std::vector<inp::ActionBinding>> ActionBinding;

        struct Data
        {
            std::vector<gfx::io::Material::Ptr> materials;
            std::string initial_scene_path;
            std::vector<ActionBinding> action_bindings;
            float sensitivity = 0.0f;
        };

        Project(Manager* manager, ResourceId id, Loader<Data>::Ptr loader);
        ~Project() override;

        Project(const Project&) = delete;
        Project& operator=(const Project&) = delete;

        [[nodiscard]] const std::vector<gfx::io::Material::Ptr>& materials() const {return materials_;}
        [[nodiscard]] ResourceId initial_scene() const {return initial_scene_;}
        [[nodiscard]] const std::vector<ActionBinding>& action_bindings() const {return action_bindings_;}
        [[nodiscard]] float sensitivity() const {return sensitivity_;}

        void load() noexcept override;

    protected:
        Loader<Data>::Ptr loader_;
        std::vector<gfx::io::Material::Ptr> materials_;
        ResourceId initial_scene_;
        std::vector<ActionBinding> action_bindings_;
        float sensitivity_;
    };
}