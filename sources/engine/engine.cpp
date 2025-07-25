#include "pch.h"
#include <nasral/engine.h>

#include "nasral/resources/material.h"

namespace nasral
{
    Engine::Engine()= default;

    Engine::~Engine(){
        shutdown();
    }

    bool Engine::initialize(const Config& config) noexcept{
        try{
            logger_ = std::make_unique<logging::Logger>(config.log);
            logger()->info("Logger initialized.");

            renderer_ = std::make_unique<rendering::Renderer>(this, config.rendering);
            logger()->info("Renderer initialized.");

            resource_manager_ = std::make_unique<resources::ResourceManager>(this, config.resources);
            logger()->info("Resource manager initialized.");

            test_scene_ = std::make_unique<TestScene>(resource_manager_);
            logger()->info("Test scene initialized.");

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

    void Engine::update(const float delta) const noexcept{
        assert(logger_ != nullptr);
        assert(resource_manager_ != nullptr);
        assert(renderer_ != nullptr);

        try{
            // Тестирование рендеринга
            renderer_->cmd_begin_frame();
            test_scene_->render(renderer_);
            renderer_->cmd_end_frame();

            // Обновление состояния ресурсов
            resource_manager_->update(delta);
        }
        catch(const std::exception& e){
            logger()->error(e.what());
        }
    }

    void Engine::shutdown() noexcept{
        try{
            if (test_scene_){
                renderer_->cmd_wait_for_frame();
                test_scene_.reset();
                resource_manager_->update(0.0f);
                logger()->info("Test scene destroyed.");
            }

            if (resource_manager_){
                resource_manager_.reset();
                logger()->info("Resource manager destroyed.");
            }

            if (renderer_){
                renderer_.reset();
                logger()->info("Renderer destroyed.");
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

    /* ТОЛЬКО ДЛЯ ТЕСТИРОВАНИЯ */

    Engine::TestScene::TestScene(const resources::ResourceManager::Ptr& resource_manager)
        : material_ref(resource_manager.get(), resources::Type::eMaterial, "materials/triangle/material.xml")
        , mesh_ref(resource_manager.get(), resources::Type::eMesh, "meshes/quad/quad.obj")
    {
        // Запрос материала
        material_ref.set_callback([&](const resources::IResource* res){
            if (res->status() == resources::Status::eLoaded){
                auto* material = dynamic_cast<const resources::Material*>(res);
                assert(material != nullptr);
                material_handles = material->render_handles();
            }
        });
        material_ref.request();

        // Запрос геометрии
        mesh_ref.set_callback([&](const resources::IResource* res){
            if (res->status() == resources::Status::eLoaded){
                auto* mesh = dynamic_cast<const resources::Mesh*>(res);
                assert(mesh != nullptr);
                mesh_handles = mesh->render_handles();
            }
        });
        mesh_ref.request();
    }

    Engine::TestScene::~TestScene(){
        material_handles = {};
        mesh_handles = {};
        material_ref.release();
        mesh_ref.release();
    }

    void Engine::TestScene::render(const rendering::Renderer::Ptr& renderer) const{
        if (!mesh_handles || !material_handles){
            return;
        }

        renderer->cmd_bind_material_pipeline(material_handles);
        renderer->cmd_draw_mesh(mesh_handles);
    }
}
