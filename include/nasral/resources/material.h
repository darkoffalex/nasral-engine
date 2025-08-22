#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <nasral/resources/ref.h>
#include <nasral/resources/resource_types.h>
#include <nasral/rendering/rendering_types.h>

namespace nasral::resources
{
    class Material final : public IResource
    {
    public:
        typedef std::unique_ptr<Material> Ptr;

        struct Data
        {
            std::string vert_shader_path;
            std::string frag_shader_path;
            std::string geom_shader_path;
            std::string type_name;
            std::string polygon_mode;
            float line_width;
        };

        Material(ResourceManager* manager, const std::string_view& path, std::unique_ptr<Loader<Data>> loader);
        ~Material() override;

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        void load() noexcept override;
        [[nodiscard]] const vk::Pipeline& vk_pipeline() const {return *vk_pipeline_;}
        [[nodiscard]] rendering::Handles::Material render_handles() const;
        [[nodiscard]] rendering::MaterialType material_type() const {return material_type_;}
        [[nodiscard]] const std::string_view& path() const {return path_;}

    private:
        void try_init_vk_objects();

    protected:
        rendering::MaterialType material_type_;
        vk::PolygonMode vk_polygon_mode_;
        float vk_line_width_;
        std::string_view path_;
        std::unique_ptr<Loader<Data>> loader_;
        Ref vert_shader_res_;
        Ref frag_shader_res_;
        Ref geom_shader_res_;
        std::optional<vk::ShaderModule> vk_vert_shader_;
        std::optional<vk::ShaderModule> vk_frag_shader_;
        std::optional<vk::ShaderModule> vk_geom_shader_;
        vk::UniquePipeline vk_pipeline_;
    };
}