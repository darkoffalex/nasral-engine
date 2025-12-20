#pragma once
#include <pugixml.hpp>
#include <magic_enum/magic_enum.hpp>
#include <nasral/res/resources/project.h>

namespace nasral::res
{
    class ProjectXmlLoader final : public Loader<Project::Data>
    {
    public:
        std::optional<Project::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            Project::Data data = {};

            // TODO: Загрузить из файла

            error_ = Error::eNone;
            return std::optional{std::move(data)};
        }
    };
}