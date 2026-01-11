#pragma once

#include <nasral/core/subsystem.h>
#include <nasral/scn/types.h>
#include <nasral/log/loggable.h>
#include <nasral/ecs/entity.h>
#include <nasral/evt/types.h>

namespace nasral::scn
{
    class Manager final : public core::Subsystem<Config>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;

        Manager(Engine* e, const Config& cfg);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        [[nodiscard]] ecs::EntityId root() const noexcept { return root_; }
        [[nodiscard]] ecs::EntityId camera() const noexcept { return camera_; }

        void set_parent(const ecs::EntityId& child
            , const ecs::EntityId& parent
            , bool keep_order_on_erase = true) const;

        void unparent(const ecs::EntityId& node
            , bool keep_order = true) const;

    private:
        void init_root();
        void init_camera();
        void on_project_loaded(const evt::Arg& arg) const;

    protected:
        // Корень сцены
        ecs::EntityId root_;
        ecs::EntityId camera_;

        // События
        evt::ListenerHandle evt_h_proj_load_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(scn::Manager)