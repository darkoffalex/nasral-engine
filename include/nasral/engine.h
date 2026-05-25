#pragma once

#include <nasral/log/logger.h>
#include <nasral/evt/manager.h>
#include <nasral/ecs/manager.h>
#include <nasral/res/manager.h>
#include <nasral/gfx/manager.h>
#include <nasral/run/manager.h>

namespace nasral
{
    struct Config
    {
        log::Config log = {};
        ecs::Config ecs = {};
        res::Config res = {};
        gfx::Config gfx = {};
        run::Config run = {};
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

        [[nodiscard]] log::Logger* logger() const noexcept;
        [[nodiscard]] evt::Manager* events() const noexcept;
        [[nodiscard]] ecs::Manager* ecs() const noexcept;
        [[nodiscard]] res::Manager* res() const noexcept;
        [[nodiscard]] gfx::Manager* gfx() const noexcept;
        [[nodiscard]] run::Manager* run() const noexcept;

    protected:
        using SubsystemTuple = std::tuple<
            log::Logger::Ptr,
            evt::Manager::Ptr,
            ecs::Manager::Ptr,
            res::Manager::Ptr,
            gfx::Manager::Ptr,
            run::Manager::Ptr>;

        SubsystemTuple subsystems_;
    };
}
