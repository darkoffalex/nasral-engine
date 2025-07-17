#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <nasral/resources/ref.h>
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    class Material : public IResource
    {
    public:
        typedef std::unique_ptr<Material> Ptr;
        explicit Material(ResourceManager* manager, const std::string_view& path);
        ~Material() override;

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        void load() noexcept override;
        [[nodiscard]] const vk::Pipeline& vk_pipeline() const {return *vk_pipeline_;}
        [[nodiscard]] const vk::PipelineLayout& vk_pipeline_layout() const {return *vk_pipeline_layout_;}

    private:
        void try_init_vk_objects();

    protected:
        std::string_view path_;
        Ref vert_shader_res_;
        Ref frag_shader_res_;
        std::optional<vk::ShaderModule> vk_vert_shader_;
        std::optional<vk::ShaderModule> vk_frag_shader_;
        vk::UniqueDescriptorSetLayout vk_dset_layout_view_;
        vk::UniqueDescriptorSetLayout vk_dset_layout_object_;
        vk::UniquePipelineLayout vk_pipeline_layout_;
        vk::UniquePipeline vk_pipeline_;
    };
}