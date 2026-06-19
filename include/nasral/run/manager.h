#pragma once

#include <nasral/common/subsystem.h>
#include <nasral/run/types.h>
#include <nasral/evt/objects/listener.h>
#include <nasral/log/loggable.h>

namespace nasral::run
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
        void on_update(float delta);
        void on_finalize();

        [[nodiscard]] const auto& state() const noexcept { return state_; }

    protected:
        void on_res_registry_changed(const evt::Arg& arg);
        void on_mat_registry_changed(const evt::Arg& arg);

    private:
        EnumMask<StateFlags> state_;
        evt::Listener::Ptr evl_res_reg_;
        evt::Listener::Ptr evl_mat_reg_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(run::Manager, "RUN")
