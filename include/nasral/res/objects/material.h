#pragma once

#include <atomic>
#include <vulkan/vulkan.hpp>
#include <nasral/res/objects/resource.h>
#include <nasral/gfx/types.h>

namespace nasral::res
{
    class Material final : public Resource
    {
    public:
        typedef std::unique_ptr<Material> Ptr;

        struct Data
        {
            std::string vertex_shader = {};
            std::string fragment_shader = {};
            std::string geometry_shader = {};
            gfx::MaterialBaseType base_type = gfx::MaterialBaseType::eDummy;
            gfx::ScreenFxPassType screen_fx_pass_type = gfx::ScreenFxPassType::eFinal;
            gfx::PolygonMode polygon_mode = gfx::PolygonMode::eFill;
            float line_width = 1.0f;
        };

        Material(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~Material() override;

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        void load() noexcept override;

        [[nodiscard]] const auto& vk_pipeline() const noexcept { return *vk_pipeline_; }
        [[nodiscard]] auto base_type() const noexcept { return base_type_; }
        [[nodiscard]] auto screen_fx_pass_type() const noexcept { return screen_fx_pass_type_; }
        [[nodiscard]] auto polygon_mode() const noexcept { return polygon_mode_; }
        [[nodiscard]] auto line_width() const noexcept { return line_width_; }
        [[nodiscard]] auto render_handles() const {return gfx::handles::Material{vk_pipeline()};}
        [[nodiscard]] vk::PolygonMode vk_polygon_mode() const noexcept;

    protected:
        void release_all_sub_resources();
        void try_init_vk_pipeline();

        [[nodiscard]] VkRenderPass find_render_pass() const;
        [[nodiscard]] VkPipelineLayout find_pipeline_layout() const;

    private:
        Loader<Data>::Ptr loader_;
        gfx::MaterialBaseType base_type_;
        gfx::ScreenFxPassType screen_fx_pass_type_;
        gfx::PolygonMode polygon_mode_;
        float line_width_;
        vk::UniquePipeline vk_pipeline_;
        std::optional<ResourceId> vert_shader_id_;
        std::optional<ResourceId> frag_shader_id_;
        std::optional<ResourceId> geom_shader_id_;
        std::optional<vk::ShaderModule> vk_vert_shader_;
        std::optional<vk::ShaderModule> vk_frag_shader_;
        std::optional<vk::ShaderModule> vk_geom_shader_;
        std::atomic<uint8_t> base_shd_loads_needed_;
        std::atomic<uint8_t> geom_shd_loads_needed_;
    };
}
