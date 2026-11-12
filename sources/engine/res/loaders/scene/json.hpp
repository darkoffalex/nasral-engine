#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include <nasral/res/objects/scene.h>

#include "../../serializers/json_scn.hpp"

namespace nasral::res
{
    class SceneJsonLoader final : public Scene::Loader<Scene::Data>
    {
    public:
        explicit SceneJsonLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<Scene::Data> load(const std::string_view& path) override
        {
            Scene::Data data;

            try
            {
                std::ifstream file(path.data());
                if (!file.is_open()){
                    throw std::runtime_error("Failed to open file");
                }

                nlohmann::json json;
                file >> json;

                data.nodes = json.at("nodes").get<std::vector<scn::NodeDesc>>();

                file.close();
            }
            catch ([[maybe_unused]] const nlohmann::json::exception& e){
                set_error(Error::eBadFormat);
                return std::nullopt;
            }
            catch ([[maybe_unused]] std::exception& e){
                set_error(Error::eLoadingFailed);
                return std::nullopt;
            }

            set_error(Error::eNone);
            return std::optional{std::move(data)};
        }
    };
}
