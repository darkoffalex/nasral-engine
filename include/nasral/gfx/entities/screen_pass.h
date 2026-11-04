#pragma once

#include <nasral/ecs/entity_view.h>
#include <nasral/res/components.h>
#include <nasral/gfx/components.h>

#define GFX_SCREEN_PASS_COMPONENTS \
    ecs::UidComponent, \
    ecs::NameComponent, \
    gfx::ScreenPassComponent, \
    gfx::ShaderHandlesComponent, \
    gfx::UniformIndexComponent, \
    res::ResourcesComponent

#define GFX_SCREEN_PASS_DIRTY_TAGS \
    gfx::DirtyUnformComponent, \
    gfx::DirtyHandlesComponent, \
    ecs::DirtyStateComponent

#define GFX_SCREEN_PASS_OPT_TAGS \
    gfx::RenderComponent, \
    ecs::NextComponent, \
    ecs::PrevComponent

namespace nasral::gfx
{
    class ScreenPassView : public ecs::EntityView<GFX_SCREEN_PASS_COMPONENTS>
    {
    public:
        using EntityView::EntityView;

        enum ResIndices : size_t
        {
            eScreenPassShader = 0,
        };

        static ScreenPassView setup(ecs::Manager* ecs
            , const ecs::EntityId& id
            , const ScreenPassDesc& desc);

        [[nodiscard]] const UniqueId& uid();
        [[nodiscard]] const std::string& name();
        [[nodiscard]] const ScreenPassType& type();
        [[nodiscard]] const uint32_t& uniform_index() const;
        [[nodiscard]] const handles::ShaderProgram& shader_program() const;
        [[nodiscard]] const uniforms::ScreenPass& uniforms() const;
        [[nodiscard]] const EnumMask<FrameAttachmentType>& input_attachments() const;
        [[nodiscard]] const EnumMask<FrameAttachmentType>& output_attachments() const;
        [[nodiscard]] ecs::EntityId next_pass() const;
        [[nodiscard]] ecs::EntityId prev_pass() const;
        [[nodiscard]] bool is_ready() const;
        [[nodiscard]] bool is_resources_requested() const;

        void set_name(const std::string& name);
        void set_uniforms(const uniforms::ScreenPass& uniforms);
        void set_screen_shader_resource(const res::ResourceId& resource_id) const;
        void destroy(bool immediate = false) const;
        void request_resources(bool immediate = false) const;

    private:
        void invalidate_ubo() const;
        void invalidate_handles() const;
        void set_resource(const res::ResourceId& resource_id, size_t index) const;
    };
}