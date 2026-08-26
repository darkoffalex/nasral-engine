#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include <nasral/res/objects/project.h>
#include <nasral/gfx/types.h>
#include <nasral/inp/types.h>

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
                for (const auto& res_entry : json.at("resources")){
                    data.resources.push_back(parse_resource_entry(res_entry));
                }

                // Регистр материалов
                for (const auto& mat_entry : json.at("materials")){
                    data.materials.push_back(parse_material_entry(mat_entry));
                }

                // Регистр пост-процессинга
                for (const auto& screen_fx_entry : json.at("screen_effects")){
                    data.screen_effects.push_back(parse_screen_fx_entry(screen_fx_entry));
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
                    for (const auto& binding_entry : input_node.at("key_bindings"))
                    {
                        data.action_bindings.push_back(parse_key_binding_entry(binding_entry));
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

    private:
        static inp::ActionDesc parse_key_binding_entry(const nlohmann::json& entry)
        {
            inp::ActionDesc binding = {};
            binding.name = entry.at("action").get<std::string>();

            for (const auto& key_entry : entry.at("keys").get<std::vector<std::string>>())
            {
                if (key_entry.find("key:") != std::string::npos)
                {
                    auto key_str = key_entry.substr(key_entry.find(':') + 1);
                    if (auto key_e = magic_enum::enum_cast<inp::KeyCode>(key_str); key_e.has_value()){
                        binding.bindings.emplace_back(key_e.value());
                    }
                }
                else if (key_entry.find("mouse:") != std::string::npos)
                {
                    auto btn_str = key_entry.substr(key_entry.find(':') + 1);
                    if (auto btn_e = magic_enum::enum_cast<inp::MouseButton>(btn_str); btn_e.has_value()){
                        binding.bindings.emplace_back(btn_e.value());
                    }
                }
            }

            return binding;
        }

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

            // Обработка настроек
            if (entry.contains("settings"))
            {
                const auto& settings = entry.at("settings");
                if (desc.base_material_type == gfx::MaterialBaseType::ePhong)
                {
                    if (settings.contains("color")) {
                        const auto& c = settings.at("color").get<std::vector<float>>();
                        if (c.size() >= 4) desc.phong_settings.color = glm::vec4(c[0], c[1], c[2], c[3]);
                    }
                    if (settings.contains("ambient")) {
                        const auto& a = settings.at("ambient").get<std::vector<float>>();
                        if (a.size() >= 4) desc.phong_settings.ambient = glm::vec4(a[0], a[1], a[2], a[3]);
                    }
                    if (settings.contains("shininess")) {
                        desc.phong_settings.shininess = settings.at("shininess").get<float>();
                    }
                    if (settings.contains("specular")) {
                        desc.phong_settings.specular = settings.at("specular").get<float>();
                    }
                }
                else if (desc.base_material_type == gfx::MaterialBaseType::ePBR)
                {
                    if (settings.contains("color")) {
                        const auto& c = settings.at("color").get<std::vector<float>>();
                        if (c.size() >= 4) desc.pbr_settings.color = glm::vec4(c[0], c[1], c[2], c[3]);
                    }
                    if (settings.contains("roughness")) {
                        desc.pbr_settings.roughness = settings.at("roughness").get<float>();
                    }
                    if (settings.contains("metallic")) {
                        desc.pbr_settings.metallic = settings.at("metallic").get<float>();
                    }
                    if (settings.contains("ao")) {
                        desc.pbr_settings.ao = settings.at("ao").get<float>();
                    }
                    if (settings.contains("emission")) {
                        desc.pbr_settings.emission = settings.at("emission").get<float>();
                    }
                }
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

        static gfx::ScreenFxDesc parse_screen_fx_entry(const nlohmann::json& entry)
        {
            gfx::ScreenFxDesc desc = {};

            // Парсинг UniqueId из массива [uint64, uint64]
            const auto& uid_array = entry.at("uid").get<std::vector<uint64_t>>();
            if (uid_array.size() >= 2)
            {
                desc.unique_id.set(uid_array[0], uid_array[1]);
            }

            desc.name = entry.at("name").get<std::string>();

            // Обработка материалов (заполнение EnumArray)
            if (entry.contains("materials") && entry.at("materials").is_array())
            {
                for (const auto& mat_entry : entry.at("materials"))
                {
                    const auto type_str = mat_entry.at("type").get<std::string>();
                    auto pass_type = magic_enum::enum_cast<gfx::ScreenFxPassType>(type_str);

                    if (pass_type.has_value())
                    {
                        // Записываем путь к материалу
                        desc.material_paths[pass_type.value()] = mat_entry.at("path").get<std::string>();
                    }
                }
            }

            return desc;
        }
    };
}