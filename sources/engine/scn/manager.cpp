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
        , active_screen_fx_(this)
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

    void Manager::on_update(const float delta)
    {
        // Если сессия не запущена
        if (engine()->run()->state().has_no(run::StateFlags::eRunning))
            return;

        // Отслеживать изменение состояния активного экранного эффекта (уведомлять другие подсистемы)
        if (active_screen_fx_.dirty_state){
            // Запрошено и готово (ресурс готов, можно задать)
            if (active_screen_fx_.requested && active_screen_fx_.is_ready()){
                assert(active_screen_fx_.screen_fx_uid().has_value() && "Screen FX UID is not set");
                engine()->events()->send(
                    evt::Type::eScreenFxChanged,
                    evt::Arg{active_screen_fx_.screen_fx_uid().value()});

                active_screen_fx_.dirty_state = false;
            }
            // Не запрошено (сбросить)
            else if (!active_screen_fx_.requested){
                engine()->events()->send(
                    evt::Type::eScreenFxChanged,
                    evt::Arg{evt::ChangeReason::eRemoved});
                active_screen_fx_.dirty_state = false;
            }
        }

        // Обновление ECS
        ecs_system_->update(delta);
    }

    void Manager::on_finalize(){
        evl_session_start_.reset();
        evl_sfc_chg_.reset();
        nodes_.clear();
        active_screen_fx_.reset();
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

            // Задать экранный эффект по умолчанию
            const auto* screen_fx = engine()->gfx()->find_screen_fx(scene->screen_fx_settings().default_fx_uid);
            active_screen_fx_.set_fx(screen_fx->entity());

            // Ресурс сцены более не нужен в RAM
            engine()->res()->release(res->id());
        });
    }

    Manager::ScreenFxState::ScreenFxState(Manager* m)
        : SubsystemObject(m)
        , fx_entity(ecs::EntityId::invalid())
        , requested(false)
        , dirty_state(false)
    {}

    Manager::ScreenFxState::~ScreenFxState(){
        if (requested){
            reset();
        }
    }

    bool Manager::ScreenFxState::is_ready() const
    {
        if (!requested){
            return false;
        }

        return engine()->ecs()->is_valid(fx_entity)
            && engine()->ecs()->has<gfx::ScreenFxHandlesComponent, res::LoadedComponent>(fx_entity)
            && !engine()->ecs()->has<gfx::DirtyHandlesComponent>(fx_entity);
    }

    bool Manager::ScreenFxState::is_error() const
    {
        if (!requested){
            return false;
        }

        return engine()->ecs()->is_valid(fx_entity)
            && engine()->ecs()->has<gfx::ScreenFxHandlesComponent, res::ErrorComponent>(fx_entity);
    }

    std::optional<UniqueId> Manager::ScreenFxState::screen_fx_uid() const
    {
        using Uid = ecs::UidComponent;                   // Уникальный ID
        using Handles = gfx::ScreenFxHandlesComponent;   // Handles материала (pipeline)

        if (!is_ready()){
            return std::nullopt;
        }

        const auto& [uid, m] = engine()->ecs()->get_components<Uid, Handles>(fx_entity);
        return uid.id;
    }

    void Manager::ScreenFxState::set_fx(const ecs::EntityId& entity)
    {
        if (requested){
            reset();
        }

        fx_entity = entity;
        requested = true;
        dirty_state = true;

        assert(engine()->ecs()->is_valid(fx_entity) && "Screen FX entity is invalid");
        ecs::inc_entity_refs(engine()->ecs(), fx_entity);
    }

    void Manager::ScreenFxState::reset()
    {
        assert(engine()->ecs()->is_valid(fx_entity) && "Screen FX entity is invalid");
        ecs::dec_entity_refs(engine()->ecs(), fx_entity);
        fx_entity = ecs::EntityId::invalid();
        requested = false;
        dirty_state = true;
    }
}
