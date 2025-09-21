#include "pch.h"
#include <nasral/engine.h>

namespace nasral
{
    Engine::Engine() = default;

    Engine::~Engine(){
        if (!initialized_) return;
        shutdown();
    }

    bool Engine::initialize(const Config& config) noexcept{
        try{
            logger_ = std::make_unique<logging::Logger>(config.log);
            logger()->info("Logger initialized.");

            ecs_ = std::make_unique<ecs::EcsManager>(this, config.ecs);
            logger()->info("ECS initialized.");

            renderer_ = std::make_unique<rendering::Renderer>(this, config.rendering);
            logger()->info("Renderer initialized.");

            resource_manager_ = std::make_unique<resources::ResourceManager>(this, config.resources);
            logger()->info("Resource manager initialized.");

            resource_system_ = std::make_unique<resources::ResourceSystem>(this);
            logger()->info("Resource system initialized.");

            rendering_system_ = std::make_unique<rendering::RenderingSystem>(this);
            logger()->info("Rendering system initialized.");

            initialized_ = true;
            return true;
        }
        catch(const logging::LoggerError& e) {
            const auto& msg = "Can't initialize logger: " + std::string(e.what());
            std::cerr << msg << std::endl;
            return false;
        }
        catch (const rendering::RenderingError& e) {
            logger()->error("Can't initialize renderer: " + std::string(e.what()));
            return false;
        }
        catch (const resources::ResourceError& e) {
            logger()->error("Can't initialize resource manager: " + std::string(e.what()));
            return false;
        }
        catch(const std::exception& e){
            logger()->error(e.what());
            return false;
        }
    }

    void Engine::update([[maybe_unused]] const float delta) noexcept
    {
        if (!initialized_) return;

        assert(logger_ != nullptr);
        assert(ecs_ != nullptr);
        assert(resource_manager_ != nullptr);
        assert(renderer_ != nullptr);

        try{
            // Обновление ресурсов
            resource_system_->update(delta);

            // Обновление материалов
            rendering_system_->update(delta);

            // Рендеринг
            rendering_system_->render();
        }
        catch(const std::exception& e){
            logger()->error(e.what());
        }
    }

    void Engine::shutdown() noexcept{
        initialized_ = false;
        try{
            if (rendering_system_){
                rendering_system_.reset();
                logger()->info("Rendering system destroyed.");
            }

            if (resource_system_){
                resource_system_.reset();
                logger()->info("Resource system destroyed.");
            }

            if (resource_manager_){
                resource_manager_.reset();
                logger()->info("Resource manager destroyed.");
            }

            if (renderer_){
                renderer_.reset();
                logger()->info("Renderer destroyed.");
            }

            if (ecs_){
                ecs_.reset();
                logger()->info("ECS destroyed.");
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
