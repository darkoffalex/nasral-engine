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
            // Источники света
            for (auto& light : test_light_sources_){
                light.update();
            }

            // Обновление узлов сцены
            static float angle = 0.0f;
            angle += delta * 10.0f;

            for (auto& node : test_scene_nodes_){
                node.set_rotation({0.0f, angle, 0.0f});
                node.update();
            }

            // Обновить данные камеры
            renderer_->update_cam_ubo(0, camera_uniforms_);

            // Начало кадра
            renderer_->cmd_begin_frame();

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
                test_light_sources_.clear();
                resource_manager_->finalize();
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
        camera_uniforms_.position = glm::vec4(0.0f, 0.0f, 2.5f, 1.0f);
        camera_uniforms_.view = glm::translate(glm::mat4(1.0f), -glm::vec3(0.0f, 0.0f, 2.5f));
        camera_uniforms_.projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        // Узлы сцены
        test_scene_nodes_.reserve(1);
        test_scene_nodes_.emplace_back(this);
        test_scene_nodes_.emplace_back(this);

        // Параметры узла
        test_scene_nodes_[0].set_position({-0.6f, 0.0f, 0.0f});
        test_scene_nodes_[0].set_scale({4.0f, 4.0f, 4.0f});

        test_scene_nodes_[1].set_position({0.6f, 0.0f, 0.0f});
        test_scene_nodes_[1].set_scale({4.0f, 4.0f, 4.0f});

        // Ресурсы узла
        test_scene_nodes_[0].set_material(rendering::MaterialInstance(
            resource_manager_.get(),
            rendering::MaterialType::ePbr,
            "materials/pbr/material.xml", {
                "textures/football/fb_diff_1k.png",
                "textures/football/fb_nor_gl_1k.png",
                "textures/football/fb_rough_1k.png"
            }));

        test_scene_nodes_[1].set_material(rendering::MaterialInstance(
            resource_manager_.get(),
            rendering::MaterialType::ePhong,
            "materials/phong/material.xml", {
                "textures/football/fb_diff_1k.png",
                "textures/football/fb_nor_gl_1k.png",
                "textures/football/fb_spec_1k.png"
            }));

        test_scene_nodes_[0].set_mesh(rendering::MeshInstance(
            resource_manager_.get(),
            "meshes/football/fb.obj"));

        test_scene_nodes_[1].set_mesh(rendering::MeshInstance(
            resource_manager_.get(),
            "meshes/football/fb.obj"));

        test_scene_nodes_[0].material_instance().set_settings(rendering::ObjectPbrMatUniforms{
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            0.5f,
            0.0f
        });

        test_scene_nodes_[1].material_instance().set_settings(rendering::ObjectPhongMatUniforms{
            glm::vec4(0.7f, 0.7f, 0.7f, 1.0f),
            glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
            32.0f,
            0.5f
        });

        test_scene_nodes_[0].request_resources();
        test_scene_nodes_[1].request_resources();

        // Освещение
        test_light_sources_.reserve(2);
        test_light_sources_.emplace_back(this);
        // test_light_sources_.emplace_back(this);

        test_light_sources_[0].set_position({3.0, 2.0f, 3.0f});
        test_light_sources_[0].set_color({1.0f, 1.0f, 1.0f});
        test_light_sources_[0].set_intensity(3.0f);

        // test_light_sources_[1].set_position({0.6f, 0.0f, 3.0f});
        // test_light_sources_[1].set_color({1.0f, 1.0f, 1.0f});
        // test_light_sources_[1].set_intensity(1.0f);
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
                        , rendering::TextureSamplerType::eAnisotropic);
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
            auto& normals = uniforms.normals;
            model = glm::translate(model, spatial_settings_.position);
            model = glm::rotate(model, glm::radians(spatial_settings_.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(spatial_settings_.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(spatial_settings_.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, spatial_settings_.scale);
            normals = glm::transpose(glm::inverse(glm::mat3(model)));
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

    Engine::LightSource::LightSource(const Engine* engine)
        : engine_(engine)
        , light_index_(engine->renderer_->light_id_acquire())
        , active_(true)
    {
        state_updated_ = true;
        settings_updated_ = true;
    }

    Engine::LightSource::~LightSource(){
        engine_->renderer_->light_ids_deactivate({light_index_});
        engine_->renderer_->light_id_release(light_index_);
    }

    void Engine::LightSource::update(){
        auto& renderer = engine_->renderer_;

        if (settings_updated_){
            renderer->update_light_ubo(light_index_, light_uniforms_);
            settings_updated_ = false;
        }

        if (state_updated_){
            if (active_){
                renderer->light_ids_activate({light_index_});
            }else{
                renderer->light_ids_deactivate({light_index_});
            }
            state_updated_ = false;
        }
    }

    void Engine::LightSource::set_position(const glm::vec3& position){
        light_uniforms_.position = glm::vec4(position, 1.0f);
        settings_updated_ = true;
    }

    void Engine::LightSource::set_color(const glm::vec3& color){
        light_uniforms_.color = glm::vec4(color, 1.0f);
        settings_updated_ = true;
    }

    void Engine::LightSource::set_intensity(const float intensity){
        light_uniforms_.intensity = intensity;
        settings_updated_ = true;
    }

    void Engine::LightSource::set_radius(const float radius){
        light_uniforms_.radius = radius;
        settings_updated_ = true;
    }

    void Engine::LightSource::set_active(const bool active){
        active_ = active;
        state_updated_ = true;
    }
}
