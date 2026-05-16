#include "pch.h"
#include <nasral/engine.h>

namespace nasral
{
    Engine::Engine(const Config& config)
    {
        try
        {
            /* Подсистемы */

            auto& log = std::get<log::Logger::Ptr>(subsystems_);
            log = std::make_unique<log::Logger>(this, config.log);
            log->init();

            auto& evt = std::get<evt::Manager::Ptr>(subsystems_);
            evt = std::make_unique<evt::Manager>(this);
            evt->init();

            auto& run = std::get<run::Manager::Ptr>(subsystems_);
            run = std::make_unique<run::Manager>(this, config.run);
            run->init();

            auto& ecs = std::get<ecs::Manager::Ptr>(subsystems_);
            ecs = std::make_unique<ecs::Manager>(this, config.ecs);
            ecs->init();

            auto& gfx = std::get<gfx::Renderer::Ptr>(subsystems_);
            gfx = std::make_unique<gfx::Renderer>(this, config.gfx);
            gfx->init();

            auto& res = std::get<res::Manager::Ptr>(subsystems_);
            res = std::make_unique<res::Manager>(this, config.res);
            res->init();

            /* ECS */

            // TODO: Инициализация ECS систем
        }
        catch (const std::runtime_error& e){
            if (logger()) logger()->fatal(e.what());
            throw;
        }
    }

    Engine::~Engine()
    {
        /* ECS */

        // TODO: Уничтожение ECS систем

        /* Подсистемы */

        auto& res = std::get<res::Manager::Ptr>(subsystems_);
        res.reset();

        auto& gfx = std::get<gfx::Renderer::Ptr>(subsystems_);
        gfx.reset();

        auto& ecs = std::get<ecs::Manager::Ptr>(subsystems_);
        ecs.reset();

        auto& run = std::get<run::Manager::Ptr>(subsystems_);
        run.reset();

        auto& evt = std::get<evt::Manager::Ptr>(subsystems_);
        evt.reset();

        auto& log = std::get<log::Logger::Ptr>(subsystems_);
        log.reset();
    }

    void Engine::finalize() const
    {
        res()->finalize();
        gfx()->finalize();
        ecs()->finalize();
        run()->finalize();
        events()->finalize();
        logger()->finalize();
    }

    void Engine::update([[maybe_unused]] const float delta)
    {
        if (kDebugBuild){
            assert(res() && "Resource subsystem is null");
            assert(gfx() && "Graphics subsystem is null");
            assert(ecs() && "ECS subsystem is null");
            assert(run() && "Runtime subsystem is null");
            assert(events() && "Event subsystem is null");
        }

        // Итерации для подсистем
        run()->update(delta);
        res()->update(delta);

        // TODO: Обновление ECS систем
        // TODO: Рендеринг

        // Выполнить отложенные действия подсистем
        std::apply([](auto&&... systems) {
            (..., (void)[](auto* s) {
                s->apply_deferred();
            }(systems.get()));
        }, subsystems_);
    }

    log::Logger* Engine::logger() const noexcept{
        return std::get<log::Logger::Ptr>(subsystems_).get();
    }

    evt::Manager* Engine::events() const noexcept{
        return std::get<evt::Manager::Ptr>(subsystems_).get();
    }

    ecs::Manager* Engine::ecs() const noexcept{
        return std::get<ecs::Manager::Ptr>(subsystems_).get();
    }

    res::Manager* Engine::res() const noexcept{
        return std::get<res::Manager::Ptr>(subsystems_).get();
    }

    gfx::Renderer* Engine::gfx() const noexcept{
        return std::get<gfx::Renderer::Ptr>(subsystems_).get();
    }

    run::Manager* Engine::run() const noexcept{
        return std::get<run::Manager::Ptr>(subsystems_).get();
    }
}
