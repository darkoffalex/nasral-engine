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
        vk_shader_module_.reset();
        RES_LOG_DESTRUCTION();
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
                RES_LOG_ERROR(error_, "Failed to load shader:" + path);
                return;
            }

            const auto shader_code = data.value().code;
            const auto renderer = manager()->engine()->renderer();
            const vk::Device* vk_device = &renderer->vk_device().logical_device();

            vk_shader_module_ = vk_device->createShaderModuleUnique(
                vk::ShaderModuleCreateInfo()
                    .setCodeSize(shader_code.size() * sizeof(uint32_t))
                    .setPCode(shader_code.data()));
        }
        catch (const std::exception& e){
            status_ = Status::eError;
            error_ = Error::eVulkanError;
            RES_LOG_ERROR(error_, e.what());
            return;
        }

        status_ = Status::eLoaded;
        error_ = Error::eNone;
        RES_LOG_LOADED();
    }
}
