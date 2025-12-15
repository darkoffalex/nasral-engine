#pragma once

#include <atomic>
#include <vulkan/vulkan.hpp>
#include <nasral/res/resource.h>
#include <nasral/res/loader.h>
#include <nasral/log/loggable.h>
#include <nasral/gfx/types.h>

namespace nasral::res
{
    class Material final : public IResource, public log::Loggable<Material>
    {
    public:
        typedef std::unique_ptr<Material> Ptr;

        struct Data
        {
            std::string vert_shader_path;
            std::string frag_shader_path;
            std::string geom_shader_path;
            gfx::MaterialType type = gfx::MaterialType::eDummy;
            vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
            float line_width = 1.0f;
        };

        Material(Manager* manager, ResourceId id, Loader<Data>::Ptr loader);
        ~Material() override;

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        void load() noexcept override;
        [[nodiscard]] const vk::Pipeline& vk_pipeline() const {return *vk_pipeline_;}
        [[nodiscard]] gfx::MaterialType material_type() const {return material_type_;}

        [[nodiscard]] gfx::handles::Material render_handles() const{
            return {
                vk_pipeline()
            };
        }

    private:
        void release_all_sub_resources() const;
        void try_init_vk_pipeline();

    protected:
        Loader<Data>::Ptr loader_;
        gfx::MaterialType material_type_;
        vk::PolygonMode vk_polygon_mode_;
        float vk_line_width_;
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
