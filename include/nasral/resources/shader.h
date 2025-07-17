#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    class Shader final : public IResource
    {
    public:
        typedef std::unique_ptr<Shader> Ptr;

        explicit Shader(ResourceManager* manager, const std::string_view& path);
        ~Shader() override;

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        void load() noexcept override;
        [[nodiscard]] const vk::ShaderModule& vk_shader_module() const { return *vk_shader_module_;};

    protected:
        std::string_view path_;
        vk::UniqueShaderModule vk_shader_module_;
    };
}