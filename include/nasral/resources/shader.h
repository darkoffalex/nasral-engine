#pragma once
#include <nasral/resources/resource_types.h>
#include <vulkan/vulkan.hpp>

namespace nasral::resources
{
    class Shader final : public IResource
    {
    public:
        explicit Shader(ResourceManager* manager, const std::string_view& path);
        ~Shader() override;

        void load() override;
        [[nodiscard]] const vk::ShaderModule& vk_shader_module() const;

    protected:
        std::string_view path_;
        vk::UniqueShaderModule vk_shader_module_;
    };
}