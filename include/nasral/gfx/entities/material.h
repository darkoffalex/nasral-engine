#pragma once

#include <nasral/ecs/entity_view.h>
#include <nasral/res/components.h>
#include <nasral/gfx/components.h>

#define GFX_MATERIAL_COMPONENTS \
    ecs::UidComponent, \
    ecs::NameComponent, \
    gfx::MaterialComponent, \
    gfx::TextureHandlesComponent, \
    gfx::ShaderHandlesComponent, \
    gfx::UniformIndexComponent, \
    res::ResourcesComponent, \
    ecs::RefsCountComponent \

#define GFX_MATERIAL_DIRTY_TAGS \
    gfx::DirtyUnformComponent, \
    gfx::DirtyTexturesComponent, \
    gfx::DirtyHandlesComponent

namespace nasral::gfx
{
    class MaterialView : public ecs::EntityView<GFX_MATERIAL_COMPONENTS>
    {
    public:
        using EntityView::EntityView;

        enum ResIndices : size_t
        {
            eRasterizationShader = 0,
            eTexAlbedo           = 1,
            eTexNormal           = 2,
            eTexRoughSpec        = 3,
            eTexHeight           = 4,
            eTexMetalReflect     = 5,
            eTexAO               = 6,
            eTexEmission         = 7,
        };

        static constexpr EnumArray<TextureType, ResIndices> kTexResMap = {
            eTexAlbedo,         // key: TextureType::eAlbedoColor
            eTexNormal,         // key: TextureType::eNormal
            eTexRoughSpec,      // key: TextureType::eRoughOrSpec
            eTexHeight,         // key: TextureType::eHeight
            eTexMetalReflect,   // key: TextureType::eMetalOrReflect
            eTexAO,             // key: TextureType::eAO
            eTexEmission        // key: TextureType::eEmission
        };

        static MaterialView setup(ecs::Manager* ecs
            , const ecs::EntityId& id
            , const MaterialDesc& desc);

        void set_name(const std::string& name);
        void set_uniforms(const uniforms::Material& uniforms);
        void set_texture_sampler(TextureType type, TextureSamplerType sampler_type);
        void set_rasterization_shader_resource(const res::ResourceId& resource_id) const;
        void set_texture_resource(TextureType type, const res::ResourceId& resource_id) const;
        void destroy(bool immediate = false) const;

    private:
        void invalidate_ubo() const;
        void invalidate_handles() const;
        void invalidate_textures() const;
        void set_resource(const res::ResourceId& resource_id, size_t index) const;
    };
}