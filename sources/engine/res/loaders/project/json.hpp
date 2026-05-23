#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include <nasral/res/objects/project.h>
#include <nasral/gfx/types.h>

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

                for (const auto& mat_entry : json.at("materials")){
                    data.materials.push_back(parse_material_entry(mat_entry));
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

    private:
        static ResourceDesc parse_resource_entry(const nlohmann::json& entry)
        {
            const auto type_str = entry.at("type").get<std::string>();
            const auto path_str = entry.at("path").get<std::string>();
            std::optional<LoadParams> params = {};

            const auto type = magic_enum::enum_cast<Type>(type_str);
            if (!type.has_value()){
                throw std::runtime_error("Unknown resource type while parsing resource list: " + type_str);
            }

            switch (type.value())
            {
            case Type::eTexture:
                {
                    if (entry.contains("load_params") && !entry.at("load_params").empty()){
                        TextureLoadParams tlp;
                        auto& params_node = entry.at("load_params");
                        tlp.srgb = params_node.at("srgb").get<bool>();
                        tlp.generate_mipmaps = params_node.at("generate_mipmaps").get<bool>();
                        params = tlp;
                    }
                    break;
                }
            case Type::eMesh:
                {
                    if (entry.contains("load_params") && !entry.at("load_params").empty()){
                        MeshLoadParams mlp;
                        auto& params_node = entry.at("load_params");
                        mlp.generate_tangents = params_node.at("generate_tangents").get<bool>();
                        mlp.generate_normals = params_node.at("generate_normals").get<bool>();
                        mlp.winding_order_ccw = params_node.at("winding_order_ccw").get<bool>();
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

        static gfx::MaterialDesc parse_material_entry(const nlohmann::json& entry)
        {
            gfx::MaterialDesc desc = {};

            // Парсинг UniqueId из массива [uint64, uint64]
            const auto& uid_array = entry.at("uid").get<std::vector<uint64_t>>();
            if (uid_array.size() >= 2)
            {
                desc.unique_id.set(uid_array[0], uid_array[1]);
            }

            desc.name = entry.at("name").get<std::string>();
            desc.base_material_path = entry.at("base_material_path").get<std::string>();

            // Обработка типа базового материала
            if (entry.contains("base_material_type"))
            {
                const auto base_type_str = entry.at("base_material_type").get<std::string>();
                desc.base_material_type =
                    magic_enum::enum_cast<gfx::MaterialBaseType>(base_type_str)
                    .value_or(gfx::MaterialBaseType::eDummy);
            }

            // Обработка текстур (заполнение EnumArray)
            if (entry.contains("textures") && entry.at("textures").is_array())
            {
                for (const auto& tex_entry : entry.at("textures"))
                {
                    const auto type_str = tex_entry.at("type").get<std::string>();
                    auto tex_type = magic_enum::enum_cast<gfx::TextureType>(type_str);

                    if (tex_type.has_value())
                    {
                        // Записываем путь к текстуре
                        desc.texture_paths[tex_type.value()] = tex_entry.at("path").get<std::string>();

                        // Записываем тип семплера (фильтрацию), если указан
                        if (tex_entry.contains("filter"))
                        {
                            const auto filter_str = tex_entry.at("filter").get<std::string>();
                            desc.texture_samplers[tex_type.value()] =
                                magic_enum::enum_cast<gfx::TextureSamplerType>(filter_str)
                                .value_or(gfx::TextureSamplerType::eNearest);
                        }
                    }
                }
            }

            return desc;
        }
    };
}