#include "pch.h"
#include <nasral/engine.h>

namespace nasral
{
    Engine::Engine(const Config& config)
    {
        try
        {
            /* Подсистемы */

            logger_ = std::make_unique<log::Logger>(this, config.log);
            logger_->info("Logger initialized.");

            evt_ = std::make_unique<evt::Manager>(this);
            logger_->info("Event manager initialized.");

            ecs_ = std::make_unique<ecs::Manager>(this, config.ecs);
            logger_->info("ECS manager initialized.");

            gfx_ = std::make_unique<gfx::Renderer>(this, config.gfx);
            logger_->info("Renderer initialized.");

            res_ = std::make_unique<res::Manager>(this, config.res);
            logger_->info("Resource manager initialized.");

            /* ECS */

            // TODO: Инициализация ECS систем
        }
        catch (const std::runtime_error& e){
            if (logger_) logger()->fatal(e.what());
            throw;
        }
    }

    Engine::~Engine()
    {
        /* ECS */

        // TODO: Уничтожение ECS систем

        /* Подсистемы */

        res_.reset();
        logger_->info("Resource manager destroyed.");

        gfx_.reset();
        logger_->info("Renderer destroyed.");

        ecs_.reset();
        logger_->info("ECS manager destroyed.");

        evt_.reset();
        logger_->info("Event manager destroyed.");

        logger_.reset();
    }

    void Engine::finalize() const
    {
        gfx_->finalize();
        res_->finalize();
    }

    void Engine::update([[maybe_unused]] const float delta)
    {

    }
}
