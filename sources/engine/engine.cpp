#include "pch.h"
#include <nasral/engine.h>

namespace nasral
{
    Engine::Engine()= default;

    Engine::~Engine(){
        shutdown();
    }

    bool Engine::initialize(const Config& config){
        try{
            logger_ = std::make_unique<logging::Logger>(config.log);
            logger()->info("Logger initialized.");

            resource_manager_ = std::make_unique<resources::ResourceManager>(this, config.resources);
            logger()->info("Resource manager initialized.");

            return true;
        }
        catch(const std::exception& e){
            logger()->error(e.what());
            return false;
        }
    }

    void Engine::update(const float delta) const {
        assert(logger_ != nullptr);
        assert(resource_manager_ != nullptr);
        assert(renderer_ != nullptr);

        try{
            resource_manager_->update(delta);
        }
        catch(const std::exception& e){
            logger()->error(e.what());
        }
    }

    void Engine::shutdown(){
        try{
            if (resource_manager_){
                resource_manager_.reset();
                logger()->info("Resource manager destroyed.");
            }

            if (logger_){
                logger()->info("Destroying logger.");
                logger_.reset();
            }
        }
        catch(const std::exception& e){
            logger()->error(e.what());
        }
    }
}
