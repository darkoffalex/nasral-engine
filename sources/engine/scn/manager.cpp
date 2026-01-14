#include "pch.h"
#include <nasral/scn/manager.h>
#include <nasral/scn/components.h>
#include <nasral/evt/utils.h>
#include <nasral/res/resources/project.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Manager::Manager(Engine* e, const Config& cfg)
        : Subsystem(e, cfg)
        , root_({})
        , camera_({})
    {
        evt_h_proj_load_ = engine()->events()->register_l(
            evt::Type::eProjectResLoaded,
            evt::bind(this, &Manager::on_project_loaded));

        init_root();
        init_camera();
    }

    Manager::~Manager()
    {
        engine()->events()->unregister_l(
            evt::Type::eProjectResLoaded,
            evt_h_proj_load_);
    }

    void Manager::set_parent(const ecs::EntityId& child
        , const ecs::EntityId& parent
        , const bool keep_order_on_erase) const
    {
        auto* ecs = engine()->ecs();
        auto& child_nc = ecs->get_component<comp::Node>(child);

        // Родители совпадают - выход
        if (child_nc.parent.has_value() && child_nc.parent.value() == parent){
            assert(false && "Child already has requested parent.");
            return;
        }

        // Я свой собственный сын! (и лошади едят друг друга!)
        if (child == parent){
            assert(false && "Can't set parent to itself.");
            return;
        }

        // Удалить из списка предыдущего родителя
        unparent(child, keep_order_on_erase);

        // Добавить новому родителю компонент списка потомков (если отсутствует)
        if (!ecs->has_component<comp::NodeChildren>(parent)){
            ecs->add_component<comp::NodeChildren>(parent);
        }

        auto& parent_ncc = ecs->get_component<comp::NodeChildren>(parent);
        parent_ncc.children.push_back(child);
        child_nc.parent = parent;
    }

    void Manager::unparent(const ecs::EntityId& node
        , const bool keep_order) const
    {
        auto* ecs = engine()->ecs();
        auto& node_c = ecs->get_component<comp::Node>(node);

        if (!node_c.parent.has_value()){
            return;
        }

        // Удалить из списка предыдущего родителя
        const auto old_parent = node_c.parent.value();
        if (ecs->has_component<comp::NodeChildren>(node_c.parent.value()))
        {
            auto& pc = ecs->get_component<comp::NodeChildren>(old_parent);
            const bool removed = keep_order
                ? pc.children.erase_ordered(node)
                : pc.children.erase_unordered(node);

            (void)removed;
            assert(!removed);

            if (pc.children.empty()){
                ecs->remove_component<comp::NodeChildren>(old_parent);
            }
        }
        else
        {
            log_warn("Node has parent but parent has no NodeChildren!");
        }

        // Обнулить родителя узла
        node_c.parent = std::nullopt;
    }

    void Manager::init_root()
    {
        auto* ecs = engine()->ecs();

        root_ = ecs->spawn();
        {
            ecs->add_component<comp::Node>(root_);
            ecs->add_component<comp::NodeChildren>(root_);
            auto& node = ecs->get_component<comp::Node>(root_);
            node.uid = core::UniqueId(1, 0);
            node.parent = std::nullopt;
        }
    }

    void Manager::init_camera()
    {
        auto* ecs = engine()->ecs();
        const auto* renderer = engine()->renderer();

        camera_ = ecs->spawn();
        {
            ecs->add_component<comp::Node>(camera_);
            ecs->add_component<comp::Spatial>(camera_);
            ecs->add_component<comp::Camera>(camera_);
            ecs->add_component<gfx::comp::UniformIndex>(camera_);
            ecs->add_component<gfx::comp::UniformState>(camera_);

            auto& node = ecs->get_component<comp::Node>(camera_);
            node.uid = core::UniqueId(1, 1);
            node.type = NodeType::eCamera;

            auto& spatial = ecs->get_component<comp::Spatial>(camera_);
            spatial.position = glm::vec3(0.0f, 0.0f, 1.0f);
            spatial.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            spatial.scale = glm::vec3(1.0f, 1.0f, 1.0f);

            auto& camera = ecs->get_component<comp::Camera>(camera_);
            camera.aspect = renderer->get_rendering_aspect();
            camera.fov = 90.0f;
            camera.far = 1000.0f;
            camera.near = 0.1f;
            camera.type = CameraType::ePerspective;

            auto& ubo_id = ecs->get_component<gfx::comp::UniformIndex>(camera_);
            ubo_id.index = 0;

            auto& ubo_s = ecs->get_component<gfx::comp::UniformState>(camera_);
            ubo_s.dirty = true;

            set_parent(camera_, root_);
        }
    }

    void Manager::on_project_loaded(const evt::Arg& arg) const
    {
        auto* ecs = engine()->ecs();
        auto* r_ptr = static_cast<res::IResource*>(*std::get_if<evt::ArgPtr>(&arg));
        if (const auto* proj = dynamic_cast<res::Project*>(r_ptr))
        {
            // Если проект загружен - задать сцены по умолчанию и запросить его
            if (proj->status() == res::Status::eLoaded){
                ecs->add_component<res::comp::Descriptor>(root_);
                ecs->add_component<res::comp::Request>(root_);
                auto& desc = ecs->get_component<res::comp::Descriptor>(root_);
                desc.res_id = proj->initial_scene();
            }
            // Если ошибка загрузки проекта
            else if (proj->status() == res::Status::eError){
                log_error("Project resource loading error (" + proj->error_str() + ").");
            }
        }
    }
}
