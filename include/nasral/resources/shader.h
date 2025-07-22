#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    template<typename DataType> class Loader;

    class Shader final : public IResource
    {
    public:
        typedef std::unique_ptr<Shader> Ptr;

        struct Data
        {
            std::vector<std::uint32_t> code;
        };

        explicit Shader(
            const ResourceManager* manager,
            const std::string_view& path,
            std::unique_ptr<Loader<Data>> loader);
        ~Shader() override;

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        void load() noexcept override;
        [[nodiscard]] const vk::ShaderModule& vk_shader_module() const { return *vk_shader_module_;};

    protected:
        std::string_view path_;
        std::unique_ptr<Loader<Data>> loader_;
        vk::UniqueShaderModule vk_shader_module_;
    };
}