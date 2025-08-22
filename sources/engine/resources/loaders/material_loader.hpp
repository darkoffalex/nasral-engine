#pragma once
#include <nasral/resources/material.h>

namespace nasral::resources
{
    class MaterialLoader final : public Loader<Material::Data>
    {
    public:
        std::optional<Material::Data> load(const std::string_view& path) override{
            pugi::xml_document doc;
            if (!doc.load_file(path.data())){
                err_code_ = ErrorCode::eLoadingError;
                return std::nullopt;
            }

            const auto shaders_conf = doc.child("Material").child("Shaders");
            const auto settings_conf = doc.child("Material").child("Settings");

            if (shaders_conf.empty()){
                err_code_ = ErrorCode::eBadFormat;
                return std::nullopt;
            }

            Material::Data data = {};
            data.type_name = doc.child("Material").attribute("type").as_string("Dummy");

            for (auto shader_info : shaders_conf.children("Shader")){
                const std::string shader_stage = shader_info.attribute("stage").as_string();
                const std::string shader_path = shader_info.attribute("path").as_string();

                if (shader_stage == "vertex"){
                    data.vert_shader_path = shader_path;
                }else if (shader_stage == "fragment"){
                    data.frag_shader_path = shader_path;
                }else if (shader_stage == "geometry"){
                    data.geom_shader_path = shader_path;
                }
            }

            for (auto setting : settings_conf.children("Setting")){
                std::string name = setting.attribute("name").as_string();
                if (name == "PolygonMode"){
                    data.polygon_mode = setting.text().as_string();
                }
                if (name == "LineWidth"){
                    try{
                        data.line_width = std::stof(setting.text().as_string());
                    }catch (...){
                        err_code_ = ErrorCode::eBadFormat;
                        return std::nullopt;
                    }
                }
            }

            if (data.vert_shader_path.empty() || data.frag_shader_path.empty()){
                err_code_ = ErrorCode::eBadFormat;
                return std::nullopt;
            }

            err_code_ = ErrorCode::eNoError;
            return std::optional{std::move(data)};
        }
    };
}