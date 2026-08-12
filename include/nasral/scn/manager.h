#pragma once

#include <list>
#include <nasral/common/subsystem.h>
#include <nasral/scn/system.h>
#include <nasral/log/loggable.h>
#include <nasral/scn/types.h>
#include <nasral/scn/objects/node.h>
#include <nasral/scn/objects/camera.h>
#include <nasral/evt/objects/listener.h>

namespace nasral::scn
{
    class Manager final : public Subsystem<Manager, Config>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;
        explicit Manager(Engine* e, const Config& config);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void on_init();
        void on_update(float delta) const;
        void on_finalize();

        Node* spawn(const NodeDesc& desc);
        void remove(const Node* node);
        void remove(const UniqueId& id);
        [[nodiscard]] Node* find(const UniqueId& id) const;
        [[nodiscard]] Node* find(const std::string& name) const;

        void request_post_processing();
        void release_post_processing();
        [[nodiscard]] bool is_post_processing_ready() const;
        [[nodiscard]] bool is_post_processing_requested() const;
        [[nodiscard]] const gfx::handles::Material& post_processing_pipeline() const;

    protected:
        void on_session_start(const evt::Arg& arg);
        void on_display_surface_changed(const evt::Arg& arg) const;
        void load_initial_scene(const std::string& path);
        void set_post_processing(const ecs::EntityId& entity);

    private:
        // Список узлов
        std::list<Node::Ptr> nodes_;
        // Основная камера
        Camera* main_camera_;
        // Слушатель события начала сеанса
        evt::Listener::Ptr evl_session_start_;
        // Слушатель события смены размеров поверхности
        evt::Listener::Ptr evl_sfc_chg_;
        // ECS-система
        System::Ptr ecs_system_;

        // Активная пост-обработка сцены
        struct
        {
            ecs::EntityId pp_entity = ecs::EntityId::invalid();
            bool requested = false;
        } post_processing_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(scn::Manager, "SCN")
