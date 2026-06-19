#include "pch.h"
#include <nasral/scn/manager.h>
#include <nasral/res/objects/project.h>
#include <nasral/scn/objects/node.h>
#include <nasral/scn/objects/mesh.h>
#include <nasral/scn/objects/camera.h>
#include <nasral/evt/utils.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Manager::Manager(Engine* e, const Config& config): Subsystem(e, config){
        nodes_.reserve(config.initial_node_count);
        log_info("Initializing manager...");
    }

    Manager::~Manager(){
        log_info("Manager destroyed");
    }

    void Manager::on_init(){

        evl_session_start_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eSessionStarted,
            evt::bind(this, &Manager::on_session_start));

        log_info("Manager initialized");
    }

    void Manager::on_update([[maybe_unused]] float delta){

    }

    void Manager::on_finalize(){
        evl_session_start_.reset();
        log_info("Manager finalized");
    }

    void Manager::spawn(const NodeDesc& desc)
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
        case NodeType::eSprite:
        case NodeType::eLight:
        default:
            break;
        }
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

    void Manager::load_initial_scene([[maybe_unused]] const std::string& path)
    {
        log_info("Loading initial scene...");

        /*
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
        spawn(camera_desc);

        // Описание меша
        NodeDesc mesh_desc = {};
        mesh_desc.type = NodeType::eMesh;
        mesh_desc.name = "Cube";
        mesh_desc.unique_id = UniqueId::generate();
        mesh_desc.spatial.position = {0.0f, 0.0f, 0.0f};
        mesh_desc.spatial.scale = {1.0f, 1.0f, 1.0f};
        mesh_desc.spatial.rotation = {0.0f, 0.0f, 0.0f};
        mesh_desc.mesh.mesh_path = res::kBuiltinMeshCube;
        mesh_desc.mesh.materials = {UniqueId{0,1}};
        spawn(mesh_desc);
        */
    }
}
