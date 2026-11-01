#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include <nasral/res/objects/project.h>
#include <nasral/gfx/types.h>
#include <nasral/inp/types.h>

#include "../../serializers/json_res.hpp"
#include "../../serializers/json_gfx.hpp"
#include "../../serializers/json_inp.hpp"

namespace nasral::res
{
    class ProjectFileJsonLoader final : public ProjectFile::Loader<ProjectFile::Data>
    {
    public:
        explicit ProjectFileJsonLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<ProjectFile::Data> load(const std::string_view& path) override
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

                // Регистр ресурсов
                if (json.contains("resources")){
                    data.resources = json.at("resources").get<std::vector<ResourceDesc>>();
                }

                // Регистр материалов
                if (json.contains("materials")){
                    data.materials = json.at("materials").get<std::vector<gfx::MaterialDesc>>();
                }

                // Настройки ввода
                if (json.contains("input"))
                {
                    const auto& input_node = json.at("input");

                    // Чувствительность
                    if (input_node.contains("mouse_sensitivity")){
                        data.mouse_sensitivity = input_node.at("mouse_sensitivity").get<float>();
                    }

                    // Привязки клавиш (действия)
                    if (input_node.contains("key_bindings")){
                        data.action_bindings = input_node.at("key_bindings").get<std::vector<inp::ActionDesc>>();
                    }
                }

                data.initial_scene = json.at("initial_scene").get<std::string>();

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