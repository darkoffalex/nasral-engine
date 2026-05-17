#pragma once

#include <nasral/res/objects/resource.h>
#include <vulkan/vulkan.hpp>

namespace nasral::res
{
    class Shader final : public Resource
    {
    public:
        typedef std::unique_ptr<Shader> Ptr;

        struct Data
        {
            std::vector<uint32_t> code = {};
        };

        Shader(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~Shader() override;

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        void load() noexcept override;
    
    private:
        Loader<Data>::Ptr loader_;
        vk::UniqueShaderModule vk_shader_module_;
    };
}
