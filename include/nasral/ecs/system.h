#pragma once

#include <nasral/common/utils.h>
#include <nlohmann/detail/meta/detected.hpp>

namespace nasral::ecs
{
    template<typename Derived, class SubsystemType>
    class System
    {
    public:
        explicit System(SubsystemType* subsystem)
        : subsystem_(subsystem)
        {}

        DECLARE_DETECTOR(init)
        DECLARE_DETECTOR(update)
        DECLARE_DETECTOR(finalize)

        [[nodiscard]] auto* subsystem() const { return subsystem_; }

        void init(){
            if constexpr(nlohmann::detail::is_detected<has_init_t, Derived>::value){
                static_cast<Derived*>(this)->init();
            }
        }

        void update(float delta){
            if constexpr (nlohmann::detail::is_detected<has_update_t, Derived, float>::value){
                static_cast<Derived*>(this)->update(delta);
            }
        }

        void finalize(){
            if constexpr (nlohmann::detail::is_detected<has_finalize_t, Derived>::value){
                static_cast<Derived*>(this)->finalize();
            }
        }

    private:
        SubsystemType* const subsystem_;
    };
}