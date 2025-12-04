#include "pch.h"
#include <nasral/res/resources/shader.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Shader::Shader(Manager* manager, const ResourceId id, Loader<Data>::Ptr loader)
        : IResource(Type::eShader, id, manager)
        , loader_(std::move(loader))
    {}

    Shader::~Shader(){
        log_info("Resource ["+id_str()+"]["+type_str()+"] destroyed.");
    }

    void Shader::load() noexcept{
        assert(loader_ != nullptr);
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->path(id_, true);

        try
        {
            const auto data = loader_->load(path);
            if (!data.has_value()){
                status_ = Status::eError;
                error_ = loader_->error();
                const std::string err_type(magic_enum::enum_name(error_));
                log_error("Resource ["+id_str()+"]["+type_str()+"] error ("+err_type+"). Failed to load shader: "+path);
                return;
            }

            const auto shader_code = data.value().code;
            const vk::Device* vk_device = nullptr; /* TODO: Получить логическое устройство Vulkan*/

            vk_shader_module_ = vk_device->createShaderModuleUnique(
                vk::ShaderModuleCreateInfo()
                    .setCodeSize(shader_code.size() * sizeof(uint32_t))
                    .setPCode(shader_code.data()));
        }
        catch (const std::exception& e){
            status_ = Status::eError;
            error_ = Error::eVulkanError;

            const std::string err_type(magic_enum::enum_name(error_));
            log_error("Resource ["+id_str()+"]["+type_str()+"] error ("+err_type+"). "+std::string(e.what()));
            return;
        }

        status_ = Status::eLoaded;
        error_ = Error::eNone;
        log_info("Resource ["+id_str()+"]["+type_str()+"] loaded.");
    }

}
