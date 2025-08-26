#pragma once
#include <nasral/rendering/rendering_types.h>
#include <nasral/resources/resource_types.h>
#include <nasral/resources/material.h>
#include <nasral/resources/ref.h>

namespace nasral::rendering
{
    class MaterialInstance final : public Instance
    {
    public:
        enum ChangesType : uint32_t
        {
            eNone               = 0,
            eShadersChanged     = 1 << 0,
            eTextureChanged     = 1 << 1,
            eSettingsChanged    = 1 << 2,
            eAny = eShadersChanged | eTextureChanged | eSettingsChanged
        };

        MaterialInstance() = default;

        MaterialInstance(
            const resources::ResourceManager* manager,
            const MaterialType& type,
            const std::string& mat_path,
            const std::vector<std::string>& tex_paths = {});

        ~MaterialInstance() = default;
        MaterialInstance(const MaterialInstance&) = default;
        MaterialInstance& operator=(const MaterialInstance&) = default;
        MaterialInstance(MaterialInstance&& other) noexcept;
        MaterialInstance& operator=(MaterialInstance&& other) noexcept;

        void set_material(MaterialType type, const std::string& path, bool request = false);
        void set_texture(TextureType type, const std::string& path, bool request = false);
        void set_texture_sampler(TextureType type, TextureSamplerType sampler);
        void set_settings(const MaterialUniforms& settings);
        void request_resources();
        void release_resources();

        [[nodiscard]] const Handles::Material& mat_render_handles() const;
        [[nodiscard]] const Handles::Texture& tex_render_handles(TextureType type) const;
        [[nodiscard]] TextureSamplerType tex_sampler(TextureType type) const;
        [[nodiscard]] const std::optional<MaterialUniforms>& settings() const;
        [[nodiscard]] static resources::BuiltinResources builtin_tex_for_type(TextureType type);

    private:
        void bind_callbacks();
        void unbind_callbacks();

    protected:
        MaterialType material_type_ = MaterialType::eDummy;
        resources::Ref material_ref_;
        Handles::Material material_handles_ = {};
        std::array<resources::Ref, static_cast<uint32_t>(TextureType::TOTAL)> texture_refs_;
        std::array<Handles::Texture, static_cast<uint32_t>(TextureType::TOTAL)> texture_handles_;
        std::array<TextureSamplerType, static_cast<uint32_t>(TextureType::TOTAL)> texture_samplers_;
        std::optional<MaterialUniforms> settings_;
    };

    static_assert(std::is_move_constructible_v<MaterialInstance>, "MaterialInstance must be movable-constructible");
    static_assert(std::is_move_assignable_v<MaterialInstance>, "MaterialInstance must be movable-assignable");
}
