#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include <nasral/res/objects/project.h>

namespace nasral::res
{
    class ProjectFileJsonLoader final : public ProjectFile::Loader<ProjectFile::Data>
    {
    public:
        explicit ProjectFileJsonLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<ProjectFile::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            ProjectFile::Data data;

            try
            {
                std::ifstream file(path.data());
                if (!file.is_open()){
                    throw std::runtime_error("Failed to open file");
                }

                nlohmann::json json;
                file >> json;

                data.initial_scene = json.at("initial_scene").get<std::string>();

                // TODO: Finalize
            }
            catch (const nlohmann::json::exception& e){
                set_error(Error::eBadFormat);
                return std::nullopt;
            }
            catch (std::exception& e){
                set_error(Error::eLoadingFailed);
                return std::nullopt;
            }

            set_error(Error::eNone);
            return std::optional{std::move(data)};
        }
    };
}