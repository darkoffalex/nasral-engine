#include "pch.h"
#include <nasral/scn/manager.h>
#include <nasral/res/objects/project.h>
#include <nasral/res/objects/scene.h>
#include <nasral/scn/objects/node.h>
#include <nasral/scn/objects/mesh.h>
#include <nasral/scn/objects/camera.h>
#include <nasral/scn/objects/light.h>
#include <nasral/evt/utils.h>
#include <nasral/ecs/utils.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Manager::Manager(Engine* e, const Config& config)
        : Subsystem(e, config)
        , main_camera_(nullptr)
        , ecs_system_(std::make_unique<System>(this))
    {
        log_info("Initializing manager...");
    }

    Manager::~Manager(){
        log_info("Manager destroyed");
    }

    void Manager::on_init()
    {
        evl_session_start_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eSessionStarted,
            evt::bind(this, &Manager::on_session_start));

        // Слушать событие изменения поверхности отображения
        evl_sfc_chg_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eDisplaySurfaceChanged,
            evt::bind(this, &Manager::on_display_surface_changed));

        ecs_system_->init();

        log_info("Manager initialized");
    }

    void Manager::on_update(const float delta) const
    {
        if (engine()->run()->state().has_no(run::StateFlags::eRunning)) return;
        ecs_system_->update(delta);
    }

    void Manager::on_finalize(){
        evl_session_start_.reset();
        evl_sfc_chg_.reset();
        nodes_.clear();
        release_post_processing();
        ecs_system_->finalize();
        log_info("Manager finalized");
    }

    Node* Manager::spawn(const NodeDesc& desc)
    {
        switch (desc.type)
        {
        case NodeType::eDummy:
            nodes_.emplace_back(Node::Ptr(new Node(this, desc)));
            break;
        case NodeType::eSpatial:
            nodes_.emplace_back(Node::Ptr(new Spatial(this, desc)));
            break;
        case NodeType::eCamera:
            nodes_.emplace_back(Node::Ptr(new Camera(this, desc)));
            break;
        case NodeType::eMesh:
            nodes_.emplace_back(Node::Ptr(new Mesh(this, desc)));
            break;
        case NodeType::eLight:
            nodes_.emplace_back(Node::Ptr(new Light(this, desc)));
            break;
        default:
            return nullptr;
        }

        return nodes_.back().get();
    }

    void Manager::remove(const Node* node){
        nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(), [&](const auto& n){
            return n.get() == node;
        }), nodes_.end());
    }

    void Manager::remove(const UniqueId& id){
        nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(), [&](const Node::Ptr& n){
            const auto dv = std::get<data::DummyNodeView>(n->data_view());
            return dv.uid == id;
        }), nodes_.end());
    }

    Node* Manager::find(const UniqueId& id) const{
        for (const auto& n : nodes_){
            if (const auto dv = std::get<data::DummyNodeView>(n->data_view()); dv.uid == id){
                return n.get();
            }
        }
        return nullptr;
    }

    Node* Manager::find(const std::string& name) const{
        for (const auto& n : nodes_){
            if (const auto dv = std::get<data::DummyNodeView>(n->data_view()); dv.name == name){
                return n.get();
            }
        }
        return nullptr;
    }

    void Manager::request_post_processing(){
        if (is_post_processing_requested()){
            log_warn("Post-processing already requested");
            return;
        }

        if (!engine()->ecs()->is_valid(post_processing_.pp_entity)){
            log_warn("Invalid post-processing pipeline entity");
            return;
        }

        post_processing_.requested = true;
        ecs::inc_entity_refs(engine()->ecs(), post_processing_.pp_entity);
    }

    void Manager::release_post_processing()
    {
        if (!is_post_processing_requested()){
            log_warn("Post-processing already released");
            return;
        }

        if (!engine()->ecs()->is_valid(post_processing_.pp_entity)){
            log_warn("Invalid post-processing pipeline entity");
            return;
        }

        post_processing_.requested = false;
        ecs::dec_entity_refs(engine()->ecs(), post_processing_.pp_entity);
    }

    bool Manager::is_post_processing_ready() const
    {
        auto& e = post_processing_.pp_entity;
        return engine()->ecs()->is_valid(e)
            && engine()->ecs()->has<gfx::PostProcessHandlesComponent, res::LoadedComponent>(e)
            && !engine()->ecs()->has<gfx::DirtyHandlesComponent>(e);
    }

    bool Manager::is_post_processing_requested() const{
        return post_processing_.requested;
    }

    const gfx::handles::Material& Manager::post_processing_pipeline() const{
        auto& e = post_processing_.pp_entity;
        auto& [material] = engine()->ecs()->get_component<gfx::PostProcessHandlesComponent>(e);
        return material;
    }

    void Manager::on_session_start([[maybe_unused]] const evt::Arg& arg)
    {
        // Получить ресурс файла проекта
        const auto proj_res_id = engine()->res()->find_project().value_or(res::kInvalidResourceId);
        const auto* proj_res = engine()->res()->get<res::ProjectFile>(proj_res_id);

        // Файл проекта обязан быть загружен на этом этапе
        if constexpr (kDebugBuild){
            assert(proj_res && "Project file resource is not found");
            assert(proj_res->status() == res::Status::eLoaded && "Project file resource is not loaded");
        }

        // Загрузка начальной сцены
        load_initial_scene(proj_res->initial_scene());
    }

    void Manager::on_display_surface_changed([[maybe_unused]] const evt::Arg& arg) const
    {
        if (main_camera_ == nullptr) return;
        main_camera_->invalidate_ubo();
    }

    void Manager::load_initial_scene([[maybe_unused]] const std::string& path)
    {
        log_info("Loading initial scene...");

        // Найти ID ресурса сцены
        const auto scene_rid = engine()->res()->find(path).value_or(res::kInvalidResourceId);
        if (scene_rid == res::kInvalidResourceId){
            log_error("Initial scene ["+path+"] not found");
            throw std::runtime_error("Initial scene ["+path+"] not found");
        }

        // Запрос ресурса, инициализация сцены (распаковка) после загрузки
        engine()->res()->request(scene_rid, [&](res::Resource* res)
        {
            // Ресурс сцены
            const auto* scene = dynamic_cast<res::Scene*>(res);
            assert(scene && "Initial scene is not a scene");

            // Проверка статуса
            if (scene->status() != res::Status::eLoaded)
            {
                log_error("Failed to load initial scene ["+path+"]");
                throw std::runtime_error("Failed to load initial scene ["+path+"]");
            }

            // Создать узлы
            for (const auto& node_desc : scene->nodes())
            {
                // Добавление узла
                auto* spawned = spawn(node_desc);

                // Первая попавшаяся камера - главная
                if (node_desc.type == NodeType::eCamera && main_camera_ == nullptr){
                    main_camera_ = dynamic_cast<Camera*>(spawned);
                    assert(main_camera_ != nullptr && "Main camera is not a camera");
                }
            }

            // Найти и задать пост-процессинг сцены
            const UniqueId pp_uid(2, 0); // <-- Временно hardcoded, позже будет браться из сцены
            const auto* pp = engine()->gfx()->find_post_processing(pp_uid);
            assert(pp != nullptr && "Post-processing pipeline is not found");
            set_post_processing(pp->entity());
            request_post_processing();

            // Ресурс сцены более не нужен в RAM
            engine()->res()->release(res->id());
        });
    }

    void Manager::set_post_processing(const ecs::EntityId& entity)
    {
        // Уменьшить ссылки на предыдущий объект конвейера пост-обработки
        if (is_post_processing_requested()){
            release_post_processing();
        }

        // Установить новый объект
        post_processing_.pp_entity = entity;
        post_processing_.requested = false;
    }
}
