#pragma once

#include <nasral/core/subsystem.h>

namespace nasral::ecs
{
    template<typename Derived>
    class System : public core::Subsystem<>
    {
    public:
        void init(){
            static_cast<Derived*>(this)->init();
        }

        void update(float dt){
            static_cast<Derived*>(this)->update(dt);
        }

        void shutdown(){
            static_cast<Derived*>(this)->shutdown();
        }

    protected:
        explicit System(Engine* engine) : Subsystem<>(engine)
        {}
    };
}