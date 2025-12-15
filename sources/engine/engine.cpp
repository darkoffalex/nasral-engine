#include "pch.h"
#include <nasral/engine.h>

namespace nasral
{
    Engine::Engine(const Config& config)
    {
        logger_ = std::make_unique<log::Logger>(this, config.log);
        logger()->info("Logger initialized.");

        ecs_ = std::make_unique<ecs::Manager>(this, config.ecs);
        logger()->info("ECS manager initialized.");

        renderer_ = std::make_unique<gfx::Renderer>(this, config.gfx);
        logger()->info("Renderer initialized.");

        res_ = std::make_unique<res::Manager>(this, config.res);
        logger()->info("Resource manager initialized.");
    }

    Engine::~Engine()
    {
        if (res_){
            res_.reset();
            logger()->info("Resource manager destroyed.");
        }

        if (renderer_){
            renderer_.reset();
            logger()->info("Renderer destroyed.");
        }

        if (ecs_){
            ecs_.reset();
            logger()->info("ECS manager destroyed.");
        }

        if (logger_){
            logger_.reset();
        }
    }

    void Engine::update([[maybe_unused]] float delta)
    {
        // TODO: Обновление систем ECS

        // Загрузка/выгрузка ресурсов
        if (res_) res_->update();

        // Рендеринг
        if (renderer_)
        {
            renderer_->cmd_begin_frame();
            renderer_->cmd_bind_frame_descriptors();

            // TODO: Рендеринг сцены (ECS)

            renderer_->cmd_end_frame();
        }
    }
}
