#include "pch.h"
#include <nasral/engine.h>

#include "nasral/resources/material.h"
#include "nasral/resources/mesh.h"

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

            init_test_scene();
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

    void Engine::update(const float delta) noexcept{
        assert(logger_ != nullptr);
        assert(resource_manager_ != nullptr);
        assert(renderer_ != nullptr);

        try{
            // Начало кадра
            renderer_->cmd_begin_frame();

            // Привязать uniform-дескрипторы камеры
            renderer_->cmd_bind_cam_descriptors();

            // Отрисовка объектов
            for (size_t i = 0; i < test_scene_nodes_.size(); ++i){
                auto& node = test_scene_nodes_[i];
                if (node.is_pending_ubo_update()){
                    renderer_->update_obj_uniforms(node.uniforms(), i);
                }

                node.render(renderer_, i);
            }

            // Конец кадра
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
            if (!test_scene_nodes_.empty()){
                renderer_->cmd_wait_for_frame();
                test_scene_nodes_.clear();
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

    void Engine::init_test_scene(){
        // Камера (пока статична)
        const auto aspect = renderer_->get_rendering_aspect();
        camera_uniforms_.view = glm::identity<glm::mat4>();
        camera_uniforms_.projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
        renderer_->update_cam_uniforms(camera_uniforms_);

        // Объекты сцены
        test_scene_nodes_.reserve(2);
        for (size_t i = 0; i < 2; ++i){
            test_scene_nodes_.emplace_back(this);
        }

        // Сдвинуть влево и право
        test_scene_nodes_[0].set_position({-0.5f, 0.0f, 0.0f});
        test_scene_nodes_[1].set_position({0.5f, 0.0f, 0.0f});

        // Запросить ресурсы
        test_scene_nodes_[0].request_resources();
        test_scene_nodes_[1].request_resources();
    }

    Engine::TestNode::TestNode(const Engine* engine)
        : material_ref_(engine->resource_manager()->make_ref(resources::Type::eMaterial, "materials/uniforms/material.xml"))
        , mesh_ref_(engine->resource_manager()->make_ref(resources::Type::eMesh, "meshes/quad/quad.obj"))
    {
        material_ref_.set_callback([&](const resources::IResource* res){
            if (res->status() == resources::Status::eLoaded){
                auto* material = dynamic_cast<const resources::Material*>(res);
                assert(material != nullptr);
                material_handles_ = material->render_handles();
            }
        });

        mesh_ref_.set_callback([&](const resources::IResource* res){
            if (res->status() == resources::Status::eLoaded){
                auto* mesh = dynamic_cast<const resources::Mesh*>(res);
                assert(mesh != nullptr);
                mesh_handles_ = mesh->render_handles();
            }
        });
    }

    void Engine::TestNode::request_resources(){
        material_ref_.request();
        mesh_ref_.request();
    }

    void Engine::TestNode::release_resources(){
        material_handles_ = {};
        mesh_handles_ = {};
        material_ref_.release();
        mesh_ref_.release();
    }

    void Engine::TestNode::render(const rendering::Renderer::Ptr& renderer, const size_t obj_index) const{
        if (!mesh_handles_ || !material_handles_){
            return;
        }

        // Привязка материала (конвейера)
        renderer->cmd_bind_material_pipeline(material_handles_);

        // Привязка дескрипторов
        renderer->cmd_bind_obj_descriptors(to<std::uint32_t>(obj_index));

        // Рисование объекта
        renderer->cmd_draw_mesh(mesh_handles_);
    }

    void Engine::TestNode::set_position(const glm::vec3& position, bool update_mat){
        position_ = position;
        if (update_mat){
            update_matrix();
        }
    }

    void Engine::TestNode::set_rotation(const glm::vec3& rotation, bool update_mat){
        rotation_ = rotation;
        if (update_mat){
            update_matrix();
        }
    }

    void Engine::TestNode::set_scale(const glm::vec3& scale, bool update_mat){
        scale_ = scale;
        if (update_mat){
            update_matrix();
        }
    }

    bool Engine::TestNode::is_pending_ubo_update(const bool reset){
        const bool result = pending_ubo_update_;
        if (reset) pending_ubo_update_ = false;
        return result;
    }

    void Engine::TestNode::update_matrix(){
        auto& model = uniforms_.model;
        model = glm::mat4(1.0f);
        model = glm::translate(model, position_);
        model = glm::rotate(model, glm::radians(rotation_.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::rotate(model, glm::radians(rotation_.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation_.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, scale_);
        pending_ubo_update_ = true;
    }
}
