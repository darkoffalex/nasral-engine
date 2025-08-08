#include "pch.h"
#include <nasral/engine.h>
#include <nasral/resources/shader.h>

namespace nasral::resources
{
    Shader::Shader(const ResourceManager* manager, const std::string_view& path, std::unique_ptr<Loader<Data>> loader)
        : IResource(Type::eShader, manager, manager->engine()->logger())
        , path_(path)
        , loader_(std::move(loader))
    {}

    Shader::~Shader(){
        logger()->info("Shader resource destroyed (" + std::string(path_.data()) + ")");
    }

    void Shader::load() noexcept{
        assert(loader_ != nullptr);
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->full_path(path_.data());

        try{
            const auto data = loader_->load(path);
            if (!data.has_value()){
                status_ = Status::eError;
                err_code_ = loader_->err_code();
                if (err_code_  == ErrorCode::eCannotOpenFile){
                    logger()->error("Can't open file: " + path);
                }
                else if (err_code_ == ErrorCode::eBadFormat){
                    logger()->error("Wrong shader size: " + path);
                }
                return;
            }

            const auto shader_code = data.value().code;
            const auto& vd = resource_manager_->engine()->renderer()->vk_device();
            vk_shader_module_ = vd->logical_device().createShaderModuleUnique(
                vk::ShaderModuleCreateInfo()
                .setPCode(shader_code.data())
                .setCodeSize(shader_code.size() * sizeof(std::uint32_t)));
        }
        catch([[maybe_unused]] const std::exception& e){
            status_ = Status::eError;
            err_code_ = ErrorCode::eVulkanError;
            logger()->error("Can't create shader module: " + path);
            return;
        }

        status_ = Status::eLoaded;
        err_code_ = ErrorCode::eNoError;
        logger()->info("Shader resource loaded (" + std::string(path_.data()) + ")");
    }
}
