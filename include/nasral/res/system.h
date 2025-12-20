#pragma once
#include <nasral/ecs/system.h>
#include <nasral/log/loggable.h>

namespace nasral::res
{
    class System final : public ecs::System<System>, public log::Loggable<System>
    {
    public:
        using Ptr = std::unique_ptr<System>;
        explicit System(Engine* engine);
        ~System();

        System(const System&) = delete;
        System& operator=(const System&) = delete;

        void init();
        void update(float dt);
        void shutdown();
    };
}

namespace nasral::log
{
    class Logger;

    template <typename T>
    struct LoggerAccessor<T, std::enable_if_t<std::is_same_v<res::System, T>>> {
        static Logger* get(const T* mgr) {
            return mgr->engine()->logger();
        }
    };
}