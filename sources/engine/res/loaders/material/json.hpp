#pragma once

#include <nasral/res/objects/material.h>

namespace nasral::res
{
    class MaterialJsonLoader final : public Material::Loader<Material::Data>
    {
    public:
        explicit MaterialJsonLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<Material::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            Material::Data data = {};

            try
            {
                std::ifstream file(path.data());
                if (!file.is_open()){
                    throw std::filesystem::filesystem_error(
                        "Failed to open file",
                        path,
                        std::make_error_code(std::errc::no_such_file_or_directory));
                }

                nlohmann::json json;
                file >> json;

                using MBT = gfx::MaterialBaseType;
                auto base_type_str = json.at("base_type").get<std::string>();
                data.base_type = magic_enum::enum_cast<MBT>(base_type_str).value_or(MBT::eDummy);

                if (json.contains("shaders") && json.at("shaders").is_array())
                {
                    for (const auto& shader_entry : json.at("shaders"))
                    {
                        auto s_stage = shader_entry.at("stage").get<std::string>();
                        auto s_path = shader_entry.at("path").get<std::string>();

                        if (s_stage == "vertex") {
                            data.vertex_shader = std::move(s_path);
                        }
                        else if (s_stage == "fragment") {
                            data.fragment_shader = std::move(s_path);
                        }
                        else if (s_stage == "geometry") {
                            data.geometry_shader = std::move(s_path);
                        }
                    }
                }

                if (json.contains("settings") && json.at("settings").is_object())
                {
                    const auto& settings = json.at("settings");

                    if (settings.contains("PolygonMode"))
                    {
                        auto mode_str = settings.at("PolygonMode").get<std::string>();
                        data.polygon_mode = magic_enum::enum_cast<gfx::PolygonMode>(mode_str).value_or(gfx::PolygonMode::eFill);
                    }

                    if (settings.contains("LineWidth"))
                    {
                        data.line_width = settings.at("LineWidth").get<float>();
                    }
                }

            }
            catch ([[maybe_unused]] const nlohmann::json::exception& e){
                set_error(Error::eBadFormat);
                return std::nullopt;
            }
            catch ([[maybe_unused]] const std::filesystem::filesystem_error& e){
                set_error(Error::eCannotOpenFile);
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