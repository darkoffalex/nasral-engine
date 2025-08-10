#include "pch.h"
#include <nasral/engine.h>

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
            test_scene_nodes_.emplace_back(this);
        }

        // Задать ресурсы узлам
        test_scene_nodes_[0].set_material(rendering::MaterialInstance(
            resource_manager_.get(),
            rendering::MaterialType::eTextured,
            "materials/textured/material.xml",
            {"textures/tiles_diff.png"}));

        test_scene_nodes_[0].set_mesh(rendering::MeshInstance(
            resource_manager_.get(), "meshes/quad/quad.obj"));

        test_scene_nodes_[1].set_material(rendering::MaterialInstance(
            resource_manager_.get(),
            rendering::MaterialType::eVertexColored,
            "materials/vertex-colored/material.xml"));

        test_scene_nodes_[1].set_mesh(rendering::MeshInstance(
            resource_manager_.get(), "meshes/quad/quad.obj"));

        // Задать параметры (пространственные) узлам
        test_scene_nodes_[0].set_position({-0.6f, 0.0f, 0.0f});
        test_scene_nodes_[1].set_position({0.6f, 0.0f, 0.0f});

        // Запросить ресурсы узлов
        test_scene_nodes_[0].request_resources();
        test_scene_nodes_[1].request_resources();
    }

    Engine::TestNode::TestNode(const Engine* engine)
        : engine_(engine)
        , obj_index_(engine->renderer_->obj_id_acquire())
    {}

    Engine::TestNode::~TestNode(){
        engine_->renderer_->obj_id_release(obj_index_);
        release_resources();
    }

    void Engine::TestNode::request_resources(){
        material_.request_resources();
        mesh_.request_resources();
    }

    void Engine::TestNode::release_resources(){
        material_.release_resources();
        mesh_.release_resources();
    }

    void Engine::TestNode::update(){
        auto& renderer = engine_->renderer_;

        // Если текстуры материала были обновлены
        if (material_.check_changes(rendering::MaterialInstance::eTextureChanged, false, true)){
            for (uint32_t i = 0; i < static_cast<uint32_t>(rendering::TextureType::TOTAL); ++i){
                if (auto& th = material_.tex_render_handles(static_cast<rendering::TextureType>(i))){
                    renderer->update_obj_tex(obj_index_
                        , th
                        , static_cast<rendering::TextureType>(i)
                        , rendering::TextureSamplerType::eNearest);
                }
            }
        }

        // Если параметры материала были обновлены
        if (material_.check_changes(rendering::MaterialInstance::eSettingsChanged, false, true)){
            if (material_.settings().has_value()){
                auto& settings = material_.settings().value();
                std::visit([&](const auto& s){
                    using T = std::decay_t<decltype(s)>;
                    if constexpr (
                        std::is_same_v<T, rendering::ObjectPhongMatUniforms> ||
                        std::is_same_v<T, rendering::ObjectPbrMatUniforms>)
                    {
                        renderer->update_obj_ubo(obj_index_, s);
                    }
                }, settings);
            }
        }

        // Если параметры узла были обновлены
        if (spatial_settings_.updated){
            rendering::ObjectTransformUniforms uniforms{};
            auto& model = uniforms.model;
            model = glm::translate(model, spatial_settings_.position);
            model = glm::rotate(model, glm::radians(spatial_settings_.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(spatial_settings_.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(spatial_settings_.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            renderer->update_obj_ubo(obj_index_, uniforms);
            spatial_settings_.updated = false;
        }
    }

    void Engine::TestNode::render() const{
        auto& renderer = engine_->renderer_;

        if (!mesh_.mesh_render_handles()
            || !material_.mat_render_handles())
        {
            return;
        }

        renderer->cmd_bind_material(material_.mat_render_handles());
        renderer->cmd_draw_mesh(mesh_.mesh_render_handles(), obj_index_);
    }

    void Engine::TestNode::set_position(const glm::vec3& position){
        spatial_settings_.position = position;
        spatial_settings_.updated = true;
    }

    void Engine::TestNode::set_rotation(const glm::vec3& rotation){
        spatial_settings_.rotation = rotation;
        spatial_settings_.updated = true;
    }

    void Engine::TestNode::set_scale(const glm::vec3& scale){
        spatial_settings_.scale = scale;
        spatial_settings_.updated = true;
    }

    void Engine::TestNode::set_material(rendering::MaterialInstance instance){
        material_ = std::move(instance);
    }

    void Engine::TestNode::set_mesh(rendering::MeshInstance instance){
        mesh_ = std::move(instance);
    }
}
