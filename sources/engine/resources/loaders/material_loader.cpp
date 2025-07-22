#include "pch.h"
#include <nasral/resources/loaders/material_loader.h>

namespace nasral::resources
{
    std::optional<Material::Data> MaterialLoader::load(const std::string_view& path){
        pugi::xml_document doc;
        if (!doc.load_file(path.data())){
            err_code_ = ErrorCode::eLoadingError;
            return std::nullopt;
        }

        const auto shaders_conf = doc.child("Material").child("Shaders");
        if (shaders_conf.empty()){
            err_code_ = ErrorCode::eBadFormat;
            return std::nullopt;
        }

        Material::Data data = {};
        for (auto shader_info : shaders_conf.children("Shader")){
            const std::string shader_stage = shader_info.attribute("stage").as_string();
            const std::string shader_path = shader_info.attribute("path").as_string();

            if (shader_stage == "vertex"){
                data.vert_shader_path = shader_path;
            }else if (shader_stage == "fragment"){
                data.frag_shader_path = shader_path;
            }
        }

        if (data.vert_shader_path.empty() || data.frag_shader_path.empty()){
            err_code_ = ErrorCode::eBadFormat;
            return std::nullopt;
        }

        err_code_ = ErrorCode::eNoError;
        return std::optional{std::move(data)};
    }
}
