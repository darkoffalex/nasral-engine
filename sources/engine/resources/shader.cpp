#include "pch.h"
#include <nasral/engine.h>
#include <nasral/resources/shader.h>

namespace nasral::resources
{
    Shader::Shader(const ResourceManager* manager, const std::string_view& path)
        : IResource(Type::eShader, manager, manager->engine()->logger())
        , path_(path)
    {}

    Shader::~Shader() = default;

    void Shader::load() noexcept{
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->full_path(path_.data());

        std::ifstream file;
        file.open(path, std::ios::binary);
        if (!file.is_open()) {
            status_ = Status::eError;
            err_code_ = ErrorCode::eCannotOpenFile;
            logger()->error("Can't open file: " + path);
            return;
        }

        const std::streamsize size = file.seekg(0, std::ios::end).tellg();
        if (size == 0 || size % 4 != 0) {
            status_ = Status::eError;
            err_code_ = ErrorCode::eLoadingError;
            logger()->error("Wrong shader size: " + path);
            return;
        }

        std::vector<std::uint32_t> shader_code(size / 4);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(shader_code.data()), size);
        file.close();

        try{
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
    }
}
