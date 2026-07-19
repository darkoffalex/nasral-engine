#include "pch.h"
#include <nasral/scn/manager.h>
#include <nasral/res/objects/project.h>
#include <nasral/scn/objects/node.h>
#include <nasral/scn/objects/mesh.h>
#include <nasral/scn/objects/camera.h>
#include <nasral/scn/objects/light.h>
#include <nasral/evt/utils.h>
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

    Node* Manager::find(const std::string& name) const
    {
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

    void Manager::on_display_surface_changed(const evt::Arg& arg) const
    {
        const auto reason = evt::from_arg<evt::ChangeReason>(arg);
        if (reason == evt::ChangeReason::eResized)
        {
            if (main_camera_ == nullptr) return;
            main_camera_->invalidate_ubo();
        }
    }

    void Manager::load_initial_scene([[maybe_unused]] const std::string& path)
    {
        log_info("Loading initial scene...");

        // Описание камеры
        NodeDesc camera_desc = {};
        camera_desc.type = NodeType::eCamera;
        camera_desc.name = "Main camera";
        camera_desc.unique_id = UniqueId::generate();
        camera_desc.spatial.position = {0.0f, 0.0f, 2.0f};
        camera_desc.spatial.scale = {1.0f, 1.0f, 1.0f};
        camera_desc.spatial.rotation = {0.0f, 0.0f, 0.0f};
        camera_desc.camera.type = gfx::ViewType::ePerspective;
        camera_desc.camera.fov = 90.0f;
        camera_desc.camera.aspect = 1.0f;
        camera_desc.camera.near = 0.1f;
        camera_desc.camera.far = 1000.0f;
        main_camera_ = dynamic_cast<Camera*>(spawn(camera_desc));

        // Описание меша
        NodeDesc m1, m2 = {};
        m1.type = NodeType::eMesh;
        m1.name = "Chair1";
        m1.unique_id = UniqueId::generate();
        m1.spatial.position = {-0.6f, 0.0f, 0.0f};
        m1.spatial.scale = {1.5f, 1.5f, 1.5f};
        m1.spatial.rotation = {0.0f, 0.0f, 0.0f};
        m1.mesh.mesh_path = "meshes/chair/chair.obj";
        m1.mesh.materials = {UniqueId{0,6}};
        m2.type = NodeType::eMesh;
        m2.name = "Chair2";
        m2.unique_id = UniqueId::generate();
        m2.spatial.position = {0.6f, 0.0f, 0.0f};
        m2.spatial.scale = {1.5f, 1.5f, 1.5f};
        m2.spatial.rotation = {0.0f, 0.0f, 0.0f};
        m2.mesh.mesh_path = "meshes/chair/chair.obj";
        m2.mesh.materials = {UniqueId{0,7}};
        spawn(m1);
        spawn(m2);

        // Описание источников света
        NodeDesc l1, l2, l3 = {};
        l1.type = NodeType::eLight;
        l1.name = "Light1";
        l1.unique_id = UniqueId::generate();
        l1.spatial.position = {-1.0f, 0.3f, 2.0f};
        l1.spatial.scale = {1.0f, 1.0f, 1.0f};
        l1.spatial.rotation = {0.0f, 0.0f, 0.0f};
        l1.light.dynamic = false;
        l1.light.is_active = true;
        l1.light.radius = 1.0f;
        l1.light.type = gfx::LightType::ePointLight;
        l1.light.color = glm::vec4(1.0f);
        l1.light.intensity = 1.0f;
        l1.light.quadratic = 0.1f;

        l2.type = NodeType::eLight;
        l2.name = "Light2";
        l2.unique_id = UniqueId::generate();
        l2.spatial.position = {1.0f, 0.3f, 2.0f};
        l2.spatial.scale = {1.0f, 1.0f, 1.0f};
        l2.spatial.rotation = {0.0f, 0.0f, 0.0f};
        l2.light.dynamic = false;
        l2.light.is_active = true;
        l2.light.radius = 1.0f;
        l2.light.type = gfx::LightType::ePointLight;
        l2.light.color = glm::vec4(1.0f);
        l2.light.intensity = 1.0f;
        l2.light.quadratic = 0.1f;

        l3.type = NodeType::eLight;
        l3.name = "Light3";
        l3.unique_id = UniqueId::generate();
        l3.spatial.position = {0.0f, -0.3f, 2.0f};
        l3.spatial.scale = {1.0f, 1.0f, 1.0f};
        l3.spatial.rotation = {0.0f, 0.0f, 0.0f};
        l3.light.dynamic = false;
        l3.light.is_active = true;
        l3.light.radius = 1.0f;
        l3.light.type = gfx::LightType::ePointLight;
        l3.light.color = glm::vec4(1.0f);
        l3.light.intensity = 1.0f;
        l3.light.quadratic = 0.1f;

        spawn(l1);
        spawn(l2);
        spawn(l3);
    }
}
