#pragma once

#include <shared_mutex>
#include <nasral/common/subsystem.h>
#include <nasral/evt/types.h>
#include <nasral/log/loggable.h>

namespace nasral::evt
{
    class Manager final : public Subsystem<Manager>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;

        explicit Manager(Engine* e);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void init();
        void finalize() const;

        ListenerHandle register_listener_unsafe(Type type, ListenerCallback callback);
        ListenerHandle register_listener(Type type, ListenerCallback callback);
        void unregister_listener_unsafe(Type type, ListenerHandle handle);
        void unregister_listener(Type type, ListenerHandle listener);
        void send_unsafe(Type type, const Arg& arg);
        void send(Type type, const Arg& arg);
        void send_deferred(Type type, const Arg& arg, bool safe = true);
        void send_deferred(Type type, Arg&& arg, bool safe = true);

    protected:
        std::shared_mutex mtx_;
        EnumArray<Type, std::vector<ListenerCallback>> listeners_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(evt::Manager)