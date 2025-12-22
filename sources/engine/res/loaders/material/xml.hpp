#pragma once
#include <pugixml.hpp>
#include <magic_enum/magic_enum.hpp>
#include <nasral/res/resources/material.h>

namespace nasral::res
{
    class MaterialXmlLoader final : public Loader<Material::Data>
    {
    public:
        std::optional<Material::Data> load(const std::string_view& path) override
        {
            pugi::xml_document doc;
            if (!doc.load_file(path.data())){
                error_ = Error::eCannotOpenFile;
                return std::nullopt;
            }

            const auto shaders_conf = doc.child("Material").child("Shaders");
            const auto settings_conf = doc.child("Material").child("Settings");

            if (shaders_conf.empty()){
                error_ = Error::eBadFormat;
                return std::nullopt;
            }

            Material::Data data = {};

            // Прочесть тип материала
            const auto str_type = doc.child("Material").attribute("type").as_string("eDummy");
            data.type = magic_enum::enum_cast<gfx::MaterialType>(str_type).value_or(gfx::MaterialType::eDummy);

            // Прочесть пути к шейдерам
            for (auto shader_info : shaders_conf.children("Shader"))
            {
                const std::unordered_map<std::string, std::string*> map = {
                    {"vertex", &data.vert_shader_path},
                    {"fragment", &data.frag_shader_path},
                    {"geometry", &data.geom_shader_path}
                };

                const std::string shader_stage = shader_info.attribute("stage").as_string();
                const std::string shader_path = shader_info.attribute("path").as_string();

                if (map.count(shader_stage) > 0){
                    *map.at(shader_stage) = shader_path;
                }
            }

            // Прочесть настройки
            for (auto setting : settings_conf.children("Setting"))
            {
                std::string name = setting.attribute("name").as_string();
                if (name == "PolygonMode")
                {
                    const std::unordered_map<std::string, vk::PolygonMode> map = {
                        {"eFill", vk::PolygonMode::eFill},
                        {"eLine", vk::PolygonMode::eLine},
                        {"ePoint", vk::PolygonMode::ePoint}
                    };

                    const auto mode = setting.text().as_string("eFill");
                    if (map.count(mode) > 0){
                        data.polygon_mode = map.at(mode);
                    }else{
                        data.polygon_mode = vk::PolygonMode::eFill;
                    }
                }
                if (name == "LineWidth")
                {
                    try{
                        data.line_width = std::stof(setting.text().as_string());
                    }catch (...){
                        data.line_width = 1.0f;
                    }
                }
            }

            if (data.vert_shader_path.empty() || data.frag_shader_path.empty()){
                error_ = Error::eBadFormat;
                return std::nullopt;
            }

            error_ = Error::eNone;
            return std::optional{std::move(data)};
        }
    };
}