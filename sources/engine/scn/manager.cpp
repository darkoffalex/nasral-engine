#include "pch.h"
#include <nasral/scn/manager.h>
#include <nasral/res/objects/project.h>
#include <nasral/res/objects/scene.h>
#include <nasral/ecs/view.h>

#include <nasral/scn/entities/node.h>
#include <nasral/scn/entities/spatial.h>
#include <nasral/scn/entities/mesh.h>
#include <nasral/scn/entities/camera.h>
#include <nasral/scn/entities/light.h>

#include <nasral/evt/utils.h>
#include <nasral/ecs/utils.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Manager::Manager(Engine* e, const Config& config)
        : Subsystem(e, config)
        , camera_(ecs::EntityId::invalid())
        , ecs_system_(std::make_unique<System>(this))
    {
        log_info("Initializing manager...");
    }

    Manager::~Manager(){
        log_info("Manager destroyed");
    }

    void Manager::on_init()
    {
        // Слушать событие начала сеанса
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
        // Если сессия не запущена
        if (engine()->run()->state().has_no(run::StateFlags::eRunning))
            return;

        // Обновление ECS
        ecs_system_->update(delta);
    }

    void Manager::on_finalize(){
        // Сброс слушателей события
        evl_session_start_.reset();
        evl_sfc_chg_.reset();

        // Очистка сцены
        clear();

        // Финализация ECS
        ecs_system_->finalize();
        log_info("Manager finalized");
    }

    ecs::EntityId Manager::spawn(const NodeDesc &desc)
    {
        ecs::EntityId node_entity = ecs::EntityId::invalid();

        switch (desc.type) {
            case NodeType::eDummy: {
                const auto node = NodeSpawner<NodeView>::spawn(engine()->ecs(), desc);
                assert(node.has_value() && "Invalid node spawned");
                node_entity = node->id();
                break;
            }
            case NodeType::eSpatial: {
                const auto node = NodeSpawner<SpatialView>::spawn(engine()->ecs(), desc);
                assert(node.has_value() && "Invalid node spawned");
                node_entity = node->id();
                break;
            }
            case NodeType::eCamera: {
                const auto node = NodeSpawner<CameraView>::spawn(engine()->ecs(), desc);
                assert(node.has_value() && "Invalid node spawned");
                node_entity = node->id();
                break;
            }
            case NodeType::eMesh: {
                const auto node = NodeSpawner<MeshView>::spawn(engine()->ecs(), desc);
                assert(node.has_value() && "Invalid node spawned");
                node_entity = node->id();
                break;
            }
            case NodeType::eLight: {
                const auto node = NodeSpawner<LightView>::spawn(engine()->ecs(), desc);
                assert(node.has_value() && "Invalid node spawned");
                node_entity = node->id();
                break;
            }
            default:
                break;
        }

        nodes_.emplace_back(node_entity);
        return node_entity;
    }

    ecs::EntityId Manager::find(const UniqueId &id) const {
        for (auto node : engine()->ecs()->view_wrapped<NodeView>()) {
            if (node.uid() == id) {
                return node.id();
            }
        }

        /*
        for (auto e : nodes_) {
            if (auto node = engine()->ecs()->as_view<NodeView>(e); node.has_value()) {
                if (node->uid() == id) {
                    return node->id();
                }
            }
        }
        */

        return ecs::EntityId::invalid();
    }

    void Manager::remove(const ecs::EntityId &id) {
        nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(), [&](const auto& n){
            return n == id;
        }), nodes_.end());

        engine()->ecs()->add_components<ecs::DestroyComponent>(id, {});
    }

    void Manager::remove(const UniqueId &id) {
        const auto to_erase = std::remove_if(nodes_.begin(), nodes_.end(), [&](const auto& n){
            auto node = engine()->ecs()->as_view<NodeView>(n);
            return node.has_value() && node->uid() == id;
        });
        nodes_.erase(to_erase, nodes_.end());
        engine()->ecs()->add_components<ecs::DestroyComponent>(*to_erase, {});
    }

    void Manager::clear() {
        for (auto e : nodes_) {
            if (auto node = engine()->ecs()->as_view<NodeView>(e); node.has_value()) {
                node->destroy();
            }
        }
        nodes_.clear();
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
        if (camera_ == ecs::EntityId::invalid()) return;
        const auto cam_view = engine()->ecs()->as_view<CameraView>(camera_);
        cam_view->invalidate_ubo();
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
                const auto e = spawn(node_desc);

                // Считать основной первую попавшуюся камеру
                if (node_desc.type == NodeType::eCamera && camera_ == ecs::EntityId::invalid()){
                    camera_ = e;
                }
            }

            // Настройки экранного эффекта (пост-обработка)
            screen_fx_state_ = std::unique_ptr<ScreenFxState>(new ScreenFxState(this, scene->screen_fx()));

            // Ресурс сцены более не нужен в RAM
            engine()->res()->release(res->id());
        });
    }
}
