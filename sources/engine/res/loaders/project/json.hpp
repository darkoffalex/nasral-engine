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

                for (const auto& res_entry : json.at("resources")){
                    data.resources.push_back(parse_resource_entry(res_entry));
                }

                data.initial_scene = json.at("initial_scene").get<std::string>();

                file.close();
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

    private:
        static ResourceDesc parse_resource_entry(const nlohmann::json& entry)
        {
            const auto type_str = entry.at("type").get<std::string>();
            const auto path_str = entry.at("path").get<std::string>();
            std::optional<LoadParams> params = {};

            const auto type = magic_enum::enum_cast<Type>(type_str);
            if (!type){
                throw std::runtime_error("Unknown resource type while parsing resource list: " + type_str);
            }

            switch (type.value())
            {
            case Type::eTexture:
                {
                    if (!entry.at("load_params").empty()){
                        TextureLoadParams tlp;
                        tlp.srgb = entry.at("load_params").at("srgb").get<bool>();
                        tlp.generate_mipmaps = entry.at("load_params").at("generate_mipmaps").get<bool>();
                        params = tlp;
                    }
                    break;
                }
            case Type::eMesh:
                {
                    if (!entry.at("load_params").empty()){
                        MeshLoadParams mlp;
                        mlp.generate_tangents = entry.at("load_params").at("generate_tangents").get<bool>();
                        mlp.generate_normals = entry.at("load_params").at("generate_normals").get<bool>();
                        mlp.winding_order_ccw = entry.at("load_params").at("winding_order_ccw").get<bool>();
                        params = mlp;
                    }
                    break;
                }
            default:
                break;
            }

            return {
                type.value(),
                path_str,
                params
            };
        }
    };
}