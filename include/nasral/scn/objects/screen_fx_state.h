#pragma once

#include <nasral/log/loggable.h>
#include <nasral/common/subsystem.h>
#include <nasral/scn/components.h>
#include <nasral/gfx/components.h>
#include <nasral/ecs/components.h>

namespace nasral::scn
{
    class Manager;
    class ScreenFxState : public SubsystemObject<Manager>, public log::Loggable<ScreenFxState>
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<ScreenFxState> Ptr;

        struct Components
        {
            using Uid = ecs::UidComponent;
            using ScreenFx = ScreenFxComponent;
            using DirtyState = DirtyStateComponent;
            using DirtyUniform = gfx::DirtyUnformComponent;
        };

        ~ScreenFxState();
        ScreenFxState(const ScreenFxState&) = delete;
        ScreenFxState& operator=(const ScreenFxState&) = delete;

        [[nodiscard]] const ecs::EntityId& entity() const;
        [[nodiscard]] data::ScreenFxStateView data_view() const;
        [[nodiscard]] std::string info() const;

        void reset_screen_fx() const;
        void set_screen_fx(const UniqueId& screen_fx) const;
        void set_ao_type(const gfx::ScreenFxAoType& type) const;
        void set_ao_radius(glm::float32 radius) const;
        void set_ao_bias(glm::float32 bias) const;
        void set_ao_multiplier(glm::float32 multiplier) const;
        void set_ao_power_pre(glm::float32 power_pre) const;
        void set_ao_power_post(glm::float32 power_post) const;
        void set_blur_samples(glm::uint32 samples) const;
        void set_blur_base_tex_radius(glm::float32 base_tex_radius) const;
        void set_blur_base_kernel_radius(glm::float32 base_kernel_radius) const;
        void set_bloom_blur_lod(glm::float32 blur_lod) const;
        void set_bloom_intensity(glm::float32 intensity) const;
        void set_final_gamma(glm::float32 gamma) const;
        void set_final_exposure(glm::float32 exposure) const;

    protected:
        ScreenFxState(Manager* manager, const ScreenFxStateDesc& description);
        void invalidate_ubo() const;

    private:
        ecs::EntityId entity_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(scn::ScreenFxState, "SCN")