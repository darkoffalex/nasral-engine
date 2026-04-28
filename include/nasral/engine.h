#pragma once

#include <nasral/log/logger.h>
#include <nasral/evt/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/res/manager.h>
#include <nasral/gfx/renderer.h>

namespace nasral
{
    struct Config
    {
        log::Config log = {};
        ecs::Config ecs = {};
        res::Config res = {};
        gfx::Config gfx = {};
    };

    class Engine
    {
    public:
        typedef std::unique_ptr<Engine> Ptr;

        explicit Engine(const Config& config);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        void finalize() const;
        void update(float delta);

        [[nodiscard]] log::Logger* logger() const noexcept { return logger_.get(); }
        [[nodiscard]] evt::Manager* events() const noexcept { return evt_.get(); }
        [[nodiscard]] ecs::Manager* ecs() const noexcept { return ecs_.get(); }
        [[nodiscard]] res::Manager* res() const noexcept { return res_.get(); }
        [[nodiscard]] gfx::Renderer* gfx() const noexcept { return gfx_.get(); }

    protected:
        log::Logger::Ptr logger_;
        evt::Manager::Ptr evt_;
        ecs::Manager::Ptr ecs_;
        res::Manager::Ptr res_;
        gfx::Renderer::Ptr gfx_;
    };
}
