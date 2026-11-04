#include "pch.h"

#include <nasral/gfx/entities/screen_pass.h>
#include <nasral/ecs/utils.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::gfx
{
    ScreenPassView ScreenPassView::setup(ecs::Manager *ecs,
        const ecs::EntityId &id,
        const ScreenPassDesc &desc)
    {
        auto* gfx = ecs->engine()->gfx();
        auto* res = ecs->engine()->res();

        // Активные ресурсы
        res::ResourcesComponent::IdsList resources_ids{};
        res::ResourcesComponent::ActiveList resources_active{};

        // Ресурс шейдерной программы
        if (const auto mat_res_id = res->find(desc.shader_program_path); mat_res_id.has_value()){
            resources_ids[eScreenPassShader] = mat_res_id.value();
            resources_active[eScreenPassShader] = true;
        }else{
            throw std::runtime_error("Shader program resource not found (" + desc.shader_program_path + ")");
        }

        // Настройки материала
        uniforms::ScreenPass settings = {};
        if (desc.type == ScreenPassType::eAO){
            settings = uniforms::ScreenPassAo{
                desc.ao_settings.type,
                desc.ao_settings.radius,
                desc.ao_settings.bias,
                desc.ao_settings.multiplier,
                desc.ao_settings.power
            };
        }else if (desc.type == ScreenPassType::eBlur){
            settings = uniforms::ScreenPassBlur{
                desc.blur_settings.samples,
                desc.blur_settings.base_tex_radius,
                desc.blur_settings.base_kernel_radius
            };
        }else if (desc.type == ScreenPassType::eCompositing){
            settings = uniforms::ScreenPassCompositing{
                desc.compositing_settings.bloom_blur_lod,
                desc.compositing_settings.bloom_intensity,
                desc.compositing_settings.gamma,
                desc.compositing_settings.exposure
            };
        }

        EnumMask<FrameAttachmentType> input_attachments = {};
        EnumMask<FrameAttachmentType> output_attachments = {};

        for (const auto & attachment : desc.input_attachments) {
            input_attachments.set(attachment);
        }

        for (const auto & attachment : desc.output_attachments) {
            output_attachments.set(attachment);
        }

        if (!input_attachments.any() && !output_attachments.any()) {
            throw std::runtime_error("Screen pass must have at least one input or one output attachment");
        }

        // Добавить основные компоненты
        ecs->add_components_immediate<GFX_SCREEN_PASS_COMPONENTS, GFX_SCREEN_PASS_DIRTY_TAGS>(id,
            {desc.unique_id},
            {desc.name},
            {desc.type, settings, input_attachments, output_attachments},
            {},
            {gfx->screen_pass_ubo_ids().acquire()},
            {resources_ids, resources_active, {res::Status::eUnloaded}},
            {},
            {},
            {});

        // Добавить ссылку на предыдущий проход (если задан)
        if (desc.previous.has_value()) {
            ecs->add_components<ecs::PrevComponent>(id, {
                desc.previous.value(),
                ecs::EntityId::invalid()
            });
        }

        // Добавить ссылку на следующий проход (если задан)
        if (desc.next.has_value()) {
            ecs->add_components<ecs::NextComponent>(id, {
                desc.next.value(),
                ecs::EntityId::invalid()
            });
        }

        return ecs->as_view<ScreenPassView>(id).value();
    }

    const UniqueId & ScreenPassView::uid() {
        return get<ecs::UidComponent>().id;
    }

    const std::string & ScreenPassView::name() {
        return get<ecs::NameComponent>().name;
    }

    const ScreenPassType& ScreenPassView::type() {
        return get<ScreenPassComponent>().type;
    }

    const uint32_t & ScreenPassView::uniform_index() const {
        return get<UniformIndexComponent>().index;
    }

    const handles::ShaderProgram & ScreenPassView::shader_program() const {
        return get<ShaderHandlesComponent>().shader_program;
    }

    const uniforms::ScreenPass & ScreenPassView::uniforms() const {
        return get<ScreenPassComponent>().uniforms;
    }

    const EnumMask<FrameAttachmentType> & ScreenPassView::input_attachments() const {
        return get<ScreenPassComponent>().input_attachments;
    }

    const EnumMask<FrameAttachmentType> & ScreenPassView::output_attachments() const {
        return get<ScreenPassComponent>().output_attachments;
    }

    ecs::EntityId ScreenPassView::next_pass() const {
        if (engine()->ecs()->has<ecs::NextComponent>(id())) {
            return engine()->ecs()->get_component<ecs::NextComponent>(id()).entity;
        }
        return ecs::EntityId::invalid();
    }

    ecs::EntityId ScreenPassView::prev_pass() const {
        if (engine()->ecs()->has<ecs::PrevComponent>(id())) {
            return engine()->ecs()->get_component<ecs::PrevComponent>(id()).entity;
        }
        return ecs::EntityId::invalid();
    }

    bool ScreenPassView::is_ready() const {
        const auto e_id = id();
        const auto* ecs = engine()->ecs();

        return ecs->is_valid(e_id)
            && ecs->has<ShaderHandlesComponent, res::LoadedComponent>(e_id)
            && !ecs->has<DirtyHandlesComponent, ecs::DirtyStateComponent>(e_id);
    }

    bool ScreenPassView::is_resources_requested() const {
        const auto e_id = id();
        const auto* ecs = engine()->ecs();
        return ecs->has<res::RequestComponent>(e_id);
    }

    void ScreenPassView::set_name(const std::string &name) {
        get<ecs::NameComponent>().name = name;
    }

    void ScreenPassView::set_uniforms(const uniforms::ScreenPass &uniforms) {
        get<ScreenPassComponent>().uniforms = uniforms;
        invalidate_ubo();
    }

    void ScreenPassView::set_screen_shader_resource(const res::ResourceId &resource_id) const {
        // Индекс ресурса меша
        constexpr auto res_index = static_cast<size_t>(eScreenPassShader);
        // Установить ресурс
        set_resource(resource_id, res_index);
        invalidate_handles();
    }

    void ScreenPassView::destroy(const bool immediate) const {
        if (immediate) {
            engine()->ecs()->add_components_immediate<ecs::DestroyComponent>(id(), {});
        }else {
            engine()->ecs()->add_components<ecs::DestroyComponent>(id(), {});
        }
    }

    void ScreenPassView::request_resources(const bool immediate) const {
        if (immediate) {
            engine()->ecs()->add_components_immediate<res::RequestComponent>(id(), {});
        }else {
            engine()->ecs()->add_components<res::RequestComponent>(id(), {});
        }
    }

    void ScreenPassView::invalidate_ubo() const {
        engine()->ecs()->add_components<DirtyUnformComponent>(id(), {});
    }

    void ScreenPassView::invalidate_handles() const {
        engine()->ecs()->add_components<DirtyHandlesComponent>(id(), {});
    }

    void ScreenPassView::set_resource(const res::ResourceId &resource_id, const size_t index) const
    {
        // Компонент ресурсов
        auto& [ids, active, statuses] = engine()->ecs()->get_component<res::ResourcesComponent>(id());
        assert(active[index] != false);

        // Если нет изменений
        if (resource_id == ids[index]) return;

        // Если ресурс загружался и он валиден - освобождение
        if (statuses[index] != res::Status::eUnloaded &&
            ids[index] != res::kInvalidResourceId)
        {
            engine()->res()->release(ids[index]);
            statuses[index] = res::Status::eUnloaded;
        }

        // Материал не загружен
        engine()->ecs()->remove_components<res::LoadedComponent>(id());

        // Задать новый и запросить
        ids[index] = resource_id;
        engine()->ecs()->add_components<res::RequestComponent>(id(), {});
    }
}
