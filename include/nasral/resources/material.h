#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <nasral/resources/ref.h>
#include <nasral/resources/resource_types.h>
#include <nasral/rendering/rendering_types.h>

namespace nasral::resources
{
    class Material : public IResource
    {
    public:
        typedef std::unique_ptr<Material> Ptr;

        struct Data
        {
            std::string vert_shader_path;
            std::string frag_shader_path;
            glm::vec4 color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        };

        Material(ResourceManager* manager, const std::string_view& path, std::unique_ptr<Loader<Data>> loader);
        ~Material() override;

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        void load() noexcept override;
        [[nodiscard]] const vk::Pipeline& vk_pipeline() const {return *vk_pipeline_;}
        [[nodiscard]] const vk::PipelineLayout& vk_pipeline_layout() const {return *vk_pipeline_layout_;}
        [[nodiscard]] rendering::Handles::Material render_handles() const;

    private:
        void try_init_vk_objects();

    protected:
        std::string_view path_;
        std::unique_ptr<Loader<Data>> loader_;
        Ref vert_shader_res_;
        Ref frag_shader_res_;
        std::optional<vk::ShaderModule> vk_vert_shader_;
        std::optional<vk::ShaderModule> vk_frag_shader_;
        vk::UniquePipelineLayout vk_pipeline_layout_;
        vk::UniquePipeline vk_pipeline_;
    };
}