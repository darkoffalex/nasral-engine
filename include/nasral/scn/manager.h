#pragma once

#include <list>
#include <nasral/common/subsystem.h>
#include <nasral/scn/system.h>
#include <nasral/log/loggable.h>
#include <nasral/scn/types.h>
#include <nasral/evt/objects/listener.h>
#include <nasral/scn/objects/screen_fx_state.h>

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

        ecs::EntityId spawn(const NodeDesc& desc);
        [[nodiscard]] ecs::EntityId find(const UniqueId& id) const;
        void remove(const ecs::EntityId& id);
        void remove(const UniqueId& id);
        void clear();

    protected:
        void on_session_start(const evt::Arg& arg);
        void on_display_surface_changed(const evt::Arg& arg) const;
        void load_initial_scene(const std::string& path);

    private:
        // Список узлов сцены
        std::list<ecs::EntityId> nodes_;
        // Активная камера
        ecs::EntityId camera_;

        // Слушатель события начала сеанса
        evt::Listener::Ptr evl_session_start_;
        // Слушатель события смены размеров поверхности
        evt::Listener::Ptr evl_sfc_chg_;
        // ECS-система
        System::Ptr ecs_system_;
        // Активная пост-обработка сцены
        ScreenFxState::Ptr screen_fx_state_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(scn::Manager, "SCN")
