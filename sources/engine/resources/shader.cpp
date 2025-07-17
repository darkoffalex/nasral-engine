#include "pch.h"
#include <nasral/resources/shader.h>
#include <nasral/engine.h>

namespace nasral::resources
{
    Shader::Shader(ResourceManager* manager, const std::string_view& path)
    : path_(path)
    {
        resource_manager_ = SafeHandle<const ResourceManager>(manager);
        status_ = Status::eUnloaded;
        err_code_ = ErrorCode::eNoError;
        type_ = Type::eShader;
    }

    Shader::~Shader() = default;

    void Shader::load() noexcept{
        std::ifstream file;
        const auto path = manager()->full_path(path_.data());
        file.open(path, std::ios::binary);
        if (!file.is_open()) {
            status_ = Status::eError;
            err_code_ = ErrorCode::eCannotOpenFile;
            return;
        }

        const std::streamsize size = file.seekg(0, std::ios::end).tellg();
        if (size == 0 || size % 4 != 0) {
            status_ = Status::eError;
            err_code_ = ErrorCode::eLoadingError;
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
            return;
        }

        status_ = Status::eLoaded;
    }
}
