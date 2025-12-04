#pragma once
#include <nasral/types.h>
#include <nasral/log/logger.h>
#include <nasral/ecs/manager.h>
#include <nasral/res/manager.h>

namespace nasral
{
    class Engine
    {
    public:
        typedef std::unique_ptr<Engine> Ptr;

        explicit Engine(const Config& config);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        void update(float delta);

        [[nodiscard]] log::Logger* logger() const noexcept { return logger_.get(); }
        [[nodiscard]] ecs::Manager* ecs() const noexcept { return ecs_.get(); }
        [[nodiscard]] res::Manager* res() const noexcept { return res_.get(); }

    protected:
        log::Logger::Ptr logger_;
        ecs::Manager::Ptr ecs_;
        res::Manager::Ptr res_;
    };
}
