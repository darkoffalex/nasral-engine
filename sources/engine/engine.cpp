#include "pch.h"
#include <nasral/engine.h>

namespace nasral
{
    Engine::Engine()= default;

    Engine::~Engine(){
        shutdown();
    }

    bool Engine::initialize(const EngineConfig& config){
        try{
            const auto& [log_file, log_to_console] = config.log;
            logger_ = std::make_unique<logging::Logger>(log_file, log_to_console);
            LOG_INFO(context(), "Logger initialized.");

            const auto& [content_dir] = config.resources;
            resource_manager_ = std::make_unique<resources::ResourceManager>(content_dir);
            LOG_INFO(context(), "Resource manager initialized.");

            return true;
        }
        catch(const std::exception& e){
            LOG_ERROR(context(), e.what());
            return false;
        }
    }

    void Engine::update(const float delta) const {
        assert(logger_ != nullptr);
        assert(resource_manager_ != nullptr);

        try{
            resource_manager_->update(delta);
        }
        catch(const std::exception& e){
            LOG_ERROR(context(), e.what());
        }
    }

    void Engine::shutdown(){
        try{
            if (resource_manager_){
                resource_manager_.reset();
                LOG_INFO(context(), "Resource manager destroyed.");
            }

            if (logger_){
                LOG_INFO(context(), "Destroying logger.");
                logger_.reset();
            }
        }
        catch(const std::exception& e){
            LOG_ERROR(context(), e.what());
        }
    }

    EngineContext Engine::context() const {
        EngineContext context{};
        context.logger = logger_.get();
        context.resource_manager = resource_manager_.get();
        return context;
    }
}
