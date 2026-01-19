#pragma once

#include <shared_mutex>
#include <nasral/core/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/evt/types.h>

namespace nasral::evt
{
    class Manager final : public core::Subsystem<>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;

        explicit Manager(Engine* e);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        ListenerHandle register_l_unsafe(Type type, Listener listener);
        ListenerHandle register_l(Type type, Listener listener);
        void unregister_l_unsafe(Type type, ListenerHandle listener);
        void unregister_l(Type type, ListenerHandle listener);
        void send_unsafe(Type type, const Arg& arg);
        void send(Type type, const Arg& arg);
        void send_deferred(Type type, const Arg& arg, bool safe = true);
        void send_deferred(Type type, Arg&& arg, bool safe = true);

        void apply_deferred_actions();

    protected:
        typedef std::function<void(Manager&)> DeferredAction;

        std::shared_mutex mtx_;
        core::EnumArray<Type, std::vector<Listener>> listeners_;
        std::vector<DeferredAction> deferred_actions_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(evt::Manager)