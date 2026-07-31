#pragma once
#include <fstream>
#include <nasral/res/objects/project.h>

namespace nasral::res
{
    class ProjectFileBuiltinLoader final : public ProjectFile::Loader<ProjectFile::Data>
    {
    public:
        explicit ProjectFileBuiltinLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<ProjectFile::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            ProjectFile::Data data;
            data.initial_scene = "";
            data.resources = {};
            data.materials = {};
            data.action_bindings = {};
            data.mouse_sensitivity = 0.0f;

            set_error(Error::eNone);
            return std::optional{std::move(data)};
        }
    };
}