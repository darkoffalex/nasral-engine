#include "pch.h"
#include <nasral/res/objects/sahder.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Shader::Shader(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader)
        : Resource(manager, id, Type::eShader)
        , loader_(std::move(loader))
    {}

    Shader::~Shader(){
        vk_shader_module_.reset();
        RES_LOG_DESTRUCTION();
    }

    void Shader::load() noexcept{
        assert(loader_ != nullptr && "Loader is null");
        if (status() == Status::eLoaded){
            return;
        }

        const auto full_path = subsystem()->path(id(), true);

        try
        {
            const auto data = loader_->load(full_path);
            if (!data.has_value()){
                throw std::runtime_error("Failed to load shader file: " + full_path);
            }

            const auto& shader_code = data.value().code;
            const auto& vk_device = subsystem()
                ->engine()
                ->gfx()
                ->vk_device()
                .logical_device();

            vk_shader_module_ = vk_device.createShaderModuleUnique(
                vk::ShaderModuleCreateInfo()
                .setCodeSize(shader_code.size() * sizeof(uint32_t))
                .setPCode(shader_code.data())
            );

        }
        catch (const std::exception& e){
            set_status(Status::eError);
            set_error(loader_->error());
            RES_LOG_ERROR(error(), e.what());
            return;
        }

        set_status(Status::eLoaded);
        set_error(Error::eNone);
        RES_LOG_LOADED();
    }
}
