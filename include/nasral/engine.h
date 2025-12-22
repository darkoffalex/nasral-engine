#pragma once
#include <nasral/types.h>
#include <nasral/log/logger.h>
#include <nasral/ecs/manager.h>
#include <nasral/res/manager.h>
#include <nasral/gfx/renderer.h>

#include <nasral/res/system.h>
#include <nasral/gfx/system.h>

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

        void finalize() const;
        void update(float delta);

        [[nodiscard]] log::Logger* logger() const noexcept { return logger_.get(); }
        [[nodiscard]] ecs::Manager* ecs() const noexcept { return ecs_.get(); }
        [[nodiscard]] res::Manager* res() const noexcept { return res_.get(); }
        [[nodiscard]] gfx::Renderer* renderer() const noexcept { return renderer_.get(); }

    protected:
        log::Logger::Ptr logger_;
        ecs::Manager::Ptr ecs_;
        res::Manager::Ptr res_;
        gfx::Renderer::Ptr renderer_;

        gfx::System::Ptr gfx_system_;
        res::System::Ptr res_system_;
    };
}
