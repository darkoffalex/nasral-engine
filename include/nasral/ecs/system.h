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

        DECLARE_DETECTOR(on_init)
        DECLARE_DETECTOR(on_update)
        DECLARE_DETECTOR(on_finalize)

        [[nodiscard]] auto* subsystem() const { return subsystem_; }

        void init(){
            if constexpr(nlohmann::detail::is_detected<has_on_init_t, Derived>::value){
                static_cast<Derived*>(this)->on_init();
            }
        }

        void update(float delta){
            if constexpr (nlohmann::detail::is_detected<has_on_update_t, Derived, float>::value){
                static_cast<Derived*>(this)->on_update(delta);
            }
        }

        void finalize(){
            if constexpr (nlohmann::detail::is_detected<has_on_finalize_t, Derived>::value){
                static_cast<Derived*>(this)->on_finalize();
            }
        }

    private:
        SubsystemType* const subsystem_;
    };
}