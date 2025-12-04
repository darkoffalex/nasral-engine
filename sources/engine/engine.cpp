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

        res_ = std::make_unique<res::Manager>(this, config.res);
        logger()->info("Resource manager initialized.");
    }

    Engine::~Engine(){
        if (res_){
            res_.reset();
            logger()->info("Resource manager destroyed.");
        }

        if (ecs_){
            ecs_.reset();
            logger()->info("ECS manager destroyed.");
        }

        if (logger_){
            logger_.reset();
        }
    }

    void Engine::update(float delta)
    {

    }
}
