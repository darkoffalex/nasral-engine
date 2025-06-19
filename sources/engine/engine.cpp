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
            const auto& [log_file, log_to_console] = config.log_config;
            logger_ = std::make_unique<logging::Logger>(log_file, log_to_console);

            LOG_INFO(context(), "Engine initialized.");
            return true;
        }
        catch(const std::exception& e){
            LOG_ERROR(context(), e.what());
            return false;
        }
    }

    void Engine::shutdown(){
        try{
            if (logger_){
                logger_->info("Destroying logger.");
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
        return context;
    }
}
