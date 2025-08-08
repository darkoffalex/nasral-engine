#include "pch.h"
#include <nasral/engine.h>

#include "nasral/resources/material.h"
#include "nasral/resources/mesh.h"
#include "nasral/resources/texture.h"

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

            // Обновление узлов сцены
            for (auto& node : test_scene_nodes_){
                node.update();
            }

            // Обновить данные камеры
            renderer_->update_cam_ubo(0, camera_uniforms_);

            // Привязка всех необходимых дескрипторов
            renderer_->cmd_bind_frame_descriptors();

            // Рендеринг объектов сцены
            for (auto& node : test_scene_nodes_){
                node.render();
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

    void Engine::init_test_scene()
    {
        // Камера (пока статична)
        const auto aspect = renderer_->get_rendering_aspect();
        camera_uniforms_.view = glm::identity<glm::mat4>();
        camera_uniforms_.projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

        // Объекты сцены
        test_scene_nodes_.reserve(2);
        for (size_t i = 0; i < 2; ++i){
            test_scene_nodes_.emplace_back(this, renderer_->obj_id_acquire());
        }

        // Сдвинуть влево и право
        test_scene_nodes_[0].set_position({-0.6f, 0.0f, 0.0f});
        test_scene_nodes_[1].set_position({0.6f, 0.0f, 0.0f});
        test_scene_nodes_[0].texture_ref_.set_path("textures/tiles_diff.png");
        test_scene_nodes_[1].texture_ref_.set_path("textures/tiles_nor_gl.png");

        // Запросить ресурсы
        test_scene_nodes_[0].request_resources();
        test_scene_nodes_[1].request_resources();
    }

    Engine::TestNode::TestNode(const Engine* engine, const uint32_t index)
        : engine_(engine)
        , obj_index_(index)
        , material_ref_(engine->resource_manager()->make_ref(resources::Type::eMaterial, "materials/textured/material.xml"))
        , mesh_ref_(engine->resource_manager()->make_ref(resources::Type::eMesh, "meshes/quad/quad.obj"))
        , texture_ref_(engine->resource_manager()->make_ref(resources::Type::eTexture, "textures/tiles_diff.png"))
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

        texture_ref_.set_callback([&](const resources::IResource* res){
            if (res->status() == resources::Status::eLoaded){
                auto* texture = dynamic_cast<const resources::Texture*>(res);
                assert(texture != nullptr);
                texture_handles_ = texture->render_handles();
            }
        });
    }

    Engine::TestNode::~TestNode(){
        release_resources();
    }

    void Engine::TestNode::request_resources(){
        material_ref_.request();
        mesh_ref_.request();
        texture_ref_.request();
    }

    void Engine::TestNode::release_resources(){
        material_handles_ = {};
        mesh_handles_ = {};

        material_ref_.release();
        mesh_ref_.release();
        texture_ref_.release();
    }

    void Engine::TestNode::update() {
        // Получить renderer движка
        auto& renderer = engine_->renderer_;

        // Обновить трансформации в UBO
        if (pending_updates_ & eTransform){
            rendering::ObjectTransformUniforms uniforms{};
            auto& model = uniforms.model;
            model = glm::translate(model, position_);
            model = glm::rotate(model, glm::radians(rotation_.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(rotation_.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rotation_.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, scale_);
            renderer->update_obj_ubo(obj_index_, uniforms);
            pending_updates_ = static_cast<UpdateFlags>(pending_updates_ & ~eTransform);
        }

        // Обновить материал в UBO
        if (pending_updates_ & eMaterial){
            rendering::ObjectPhongMatUniforms uniforms{};
            uniforms.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // TODO: Обновление
            renderer->update_obj_ubo(obj_index_, uniforms);
            pending_updates_ = static_cast<UpdateFlags>(pending_updates_ & ~eMaterial);
        }

        if (pending_updates_ & eTextures){
            if (texture_handles_){
                renderer->update_obj_tex(
                obj_index_,
                texture_handles_,
                rendering::TextureType::eAlbedoColor,
                rendering::TextureSamplerType::eNearest);
                pending_updates_ = static_cast<UpdateFlags>(pending_updates_ & ~eTextures);
            }
        }
    }

    void Engine::TestNode::render() const{
        if (!mesh_handles_ || !material_handles_ || !texture_handles_){
            return;
        }

        // Получить renderer движка
        auto& renderer = engine_->renderer_;

        // Привязка материала
        renderer->cmd_bind_material(material_handles_);

        // Рисование объекта
        renderer->cmd_draw_mesh(mesh_handles_, obj_index_);
    }

    void Engine::TestNode::set_position(const glm::vec3& position){
        position_ = position;
        pending_updates_ = static_cast<UpdateFlags>(pending_updates_ | eTransform);
    }

    void Engine::TestNode::set_rotation(const glm::vec3& rotation){
        rotation_ = rotation;
        pending_updates_ = static_cast<UpdateFlags>(pending_updates_ | eTransform);
    }

    void Engine::TestNode::set_scale(const glm::vec3& scale){
        scale_ = scale;
        pending_updates_ = static_cast<UpdateFlags>(pending_updates_ | eTransform);
    }
}
