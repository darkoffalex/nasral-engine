#include "pch.h"

#include <nasral/scn/objects/screen_fx_state.h>
#include <nasral/gfx/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/scn/manager.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    ScreenFxState::ScreenFxState(Manager* manager, const ScreenFxStateDesc& description)
        : SubsystemObject(manager)
        , entity_(ecs::EntityId::invalid())
    {
        entity_ = engine()->ecs()->spawn();

        const auto* sfx = engine()->gfx()->find_screen_fx(description.screen_fx_uid);
        if (!sfx){
            throw std::runtime_error("Screen FX not found");
        }

        engine()->ecs()->add_components_immediate<
            Components::Uid,
            Components::ScreenFx,
            Components::DirtyState,
            Components::DirtyUniform>(entity_,
                {description.unique_id},
                {sfx->entity(), ecs::EntityId::invalid(), false, false},
                {},
                {});

        log_info("Screen FX settings registered (" + info() + ")");
    }

    ScreenFxState::~ScreenFxState()
    {
        auto* ecs = subsystem()->engine()->ecs();
        ecs->add_components<ecs::DestroyComponent>(entity_, {});
        log_info("Screen FX settings unregistered (" + info() + ")");
    }

    const ecs::EntityId& ScreenFxState::entity() const{
        return entity_;
    }

    data::ScreenFxStateView ScreenFxState::data_view() const
    {
        const auto [id, sfx] = engine()->ecs()->get_components<
            Components::Uid,
            Components::ScreenFx
            >(entity_);

        return {
            id.id,
            sfx.ao_type,
            sfx.ao_radius,
            sfx.ao_bias,
            sfx.ao_multiplier,
            sfx.ao_power_pre,
            sfx.ao_power_post,
            sfx.blur_samples,
            sfx.blur_base_tex_radius,
            sfx.blur_base_kernel_radius,
            sfx.bloom_blur_lod,
            sfx.bloom_intensity,
            sfx.gamma,
            sfx.exposure
        };
    }

    std::string ScreenFxState::info() const
    {
        const auto [uid,
            ao_type,
            ao_radius,
            ao_bias,
            ao_multiplier,
            ao_power_pre,
            ao_power_post,
            blur_samples,
            blur_base_tex_radius,
            blur_base_kernel_radius,
            bloom_blur_lod,
            bloom_intensity,
            gamma,
            exposure] = data_view();

        std::stringstream ss;
        ss << "UID: " << uid.to_string();
        ss << ", AO Type: " << magic_enum::enum_name(ao_type);
        ss << ", AO Radius: " << ao_radius;
        ss << ", AO Bias: " << ao_bias;
        ss << ", AO Multiplier: " << ao_multiplier;
        ss << ", AO Power Pre: " << ao_power_pre;
        ss << ", AO Power Post: " << ao_power_post;
        ss << ", Blur Samples: " << blur_samples;
        ss << ", Blur Base Tex Radius: " << blur_base_tex_radius;
        ss << ", Blur Base Kernel Radius: " << blur_base_kernel_radius;
        ss << ", Bloom Blur LOD: " << bloom_blur_lod;
        ss << ", Bloom Intensity: " << bloom_intensity;
        ss << ", Gamma: " << gamma;
        ss << ", Exposure: " << exposure;
        return ss.str();
    }

    void ScreenFxState::reset_screen_fx() const
    {
        // Сбросить entity экранного эффекта
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.screen_fx_prev = sfx_comp.screen_fx;
        sfx_comp.screen_fx_prev_released = false;
        sfx_comp.screen_fx = ecs::EntityId::invalid();
        sfx_comp.screen_fx_requested = false;

        // Пометить entity как измененный (грязный)
        engine()->ecs()->add_components<Components::DirtyState>(entity_, {});
    }

    void ScreenFxState::set_screen_fx(const UniqueId& screen_fx) const
    {
        // Найти entity экранного эффекта
        const auto* sfx = engine()->gfx()->find_screen_fx(screen_fx);
        if (!sfx) throw std::runtime_error("Screen FX not found");

        // Задать entity экранного эффекта
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.screen_fx_prev = sfx_comp.screen_fx;
        sfx_comp.screen_fx_prev_released = false;

        sfx_comp.screen_fx = sfx->entity();
        sfx_comp.screen_fx_requested = false;

        // Пометить entity как измененный (грязный)
        engine()->ecs()->add_components<Components::DirtyState>(entity_, {});
    }

    void ScreenFxState::set_ao_type(const gfx::ScreenFxAoType& type) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.ao_type = type;
        invalidate_ubo();
    }

    void ScreenFxState::set_ao_radius(const glm::float32 radius) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.ao_radius = radius;
        invalidate_ubo();
    }

    void ScreenFxState::set_ao_bias(const glm::float32 bias) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.ao_bias = bias;
        invalidate_ubo();
    }

    void ScreenFxState::set_ao_multiplier(const glm::float32 multiplier) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.ao_multiplier = multiplier;
        invalidate_ubo();
    }

    void ScreenFxState::set_ao_power_pre(const glm::float32 power_pre) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.ao_power_pre = power_pre;
        invalidate_ubo();
    }

    void ScreenFxState::set_ao_power_post(const glm::float32 power_post) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.ao_power_post = power_post;
        invalidate_ubo();
    }

    void ScreenFxState::set_blur_samples(const glm::uint32 samples) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.blur_samples = samples;
        invalidate_ubo();
    }

    void ScreenFxState::set_blur_base_tex_radius(const glm::float32 base_tex_radius) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.blur_base_tex_radius = base_tex_radius;
        invalidate_ubo();
    }

    void ScreenFxState::set_blur_base_kernel_radius(const glm::float32 base_kernel_radius) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.blur_base_kernel_radius = base_kernel_radius;
        invalidate_ubo();
    }

    void ScreenFxState::set_bloom_blur_lod(const glm::float32 blur_lod) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.bloom_blur_lod = blur_lod;
        invalidate_ubo();
    }

    void ScreenFxState::set_bloom_intensity(const glm::float32 intensity) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.bloom_intensity = intensity;
        invalidate_ubo();
    }

    void ScreenFxState::set_final_gamma(const glm::float32 gamma) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.gamma = gamma;
        invalidate_ubo();
    }

    void ScreenFxState::set_final_exposure(const glm::float32 exposure) const
    {
        auto& sfx_comp = engine()->ecs()->get_component<Components::ScreenFx>(entity_);
        sfx_comp.exposure = exposure;
        invalidate_ubo();
    }

    void ScreenFxState::invalidate_ubo() const
    {
        if (!engine()->ecs()->has<Components::DirtyUniform>(entity())){
            engine()->ecs()->add_components<Components::DirtyUniform>(entity(), {});
        }
    }

}
