#pragma once

#include <vulkan/vulkan.hpp>
#include <nasral/res/objects/resource.h>
#include <nasral/gfx/types.h>

namespace nasral::res
{
    class ShaderProgram final : public Resource
    {
    public:
        typedef std::unique_ptr<ShaderProgram> Ptr;

        struct Data
        {
            struct EntryPointInfo
            {
                vk::ShaderStageFlagBits stage;
                std::string name;
            };

            std::vector<uint32_t> code = {};
            gfx::ShaderProgramType program_type = gfx::ShaderProgramType::eRasterization;
            gfx::ScreenFxPassType screen_fx_pass_type = gfx::ScreenFxPassType::ePingPong;
            std::vector<EntryPointInfo> entry_points = {};
        };

        ShaderProgram(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~ShaderProgram() override;

        ShaderProgram(const ShaderProgram&) = delete;
        ShaderProgram& operator=(const ShaderProgram&) = delete;

        void load() noexcept override;

        [[nodiscard]] const auto& vk_pipeline() const noexcept { return *vk_pipeline_; }
        [[nodiscard]] const auto& program_type() const noexcept { return program_type_; }
        [[nodiscard]] const auto& screen_fx_pass_type() const noexcept { return screen_fx_pass_type_; }

    private:
        struct RenderingAttachmentFormatInfo
        {
            std::vector<vk::Format> color_formats = {};
            vk::Format depth_format = vk::Format::eUndefined;
            vk::Format stencil_format = vk::Format::eUndefined;
        };

        void init_rasterization_pipeline(
            const vk::ShaderModule& module,
            const std::vector<Data::EntryPointInfo>& entry_points);

        void init_raytracing_pipeline(
            const vk::ShaderModule& module,
            const std::vector<Data::EntryPointInfo>& entry_points);

        void init_compute_pipeline(
            const vk::ShaderModule& module,
            const std::vector<Data::EntryPointInfo>& entry_points);

        [[nodiscard]] RenderingAttachmentFormatInfo attachment_formats() const;

    protected:
        Loader<Data>::Ptr loader_;
        gfx::ShaderProgramType program_type_;
        gfx::ScreenFxPassType screen_fx_pass_type_;
        vk::UniquePipeline vk_pipeline_;
    };
}
