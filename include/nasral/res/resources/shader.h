#pragma once

#include <vulkan/vulkan.hpp>
#include <nasral/res/resource.h>
#include <nasral/res/loader.h>
#include <nasral/log/loggable.h>

namespace nasral::res
{
    class Manager;
    class Shader final : public IResource, public log::Loggable<Shader>
    {
    public:
        typedef std::unique_ptr<Shader> Ptr;

        struct Data
        {
            std::vector<std::uint32_t> code;
        };

        Shader(Manager* manager, ResourceId id, Loader<Data>::Ptr loader);
        ~Shader() override;

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        void load() noexcept override;

    protected:
        Loader<Data>::Ptr loader_;
        vk::UniqueShaderModule vk_shader_module_;
    };
}