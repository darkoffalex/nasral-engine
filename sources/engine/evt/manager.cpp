#include "pch.h"
#include <nasral/evt/manager.h>

namespace nasral::evt
{
    Manager::Manager(Engine* e) : Subsystem(e)
    {
        for (const Type t : magic_enum::enum_values<Type>()){
            listeners_[t].reserve(kInitialListenersCount);
        }
    }

    Manager::~Manager() = default;

    ListenerHandle Manager::register_listener_unsafe(const Type type, ListenerCallback callback)
    {
        auto& v = listeners_[type];
        v.emplace_back(std::move(callback));
        return v.size() - 1;
    }

    ListenerHandle Manager::register_listener(const Type type, ListenerCallback callback)
    {
        std::unique_lock lock(mtx_);
        return register_listener_unsafe(type, std::move(callback));
    }

    void Manager::unregister_listener_unsafe(const Type type, const ListenerHandle handle)
    {
        auto& v = listeners_[type];
        if (handle >= v.size()) { return; }
        v[handle] = std::move(v.back());
        v.pop_back();
    }

    void Manager::unregister_listener(const Type type, const ListenerHandle listener)
    {
        std::unique_lock lock(mtx_);
        unregister_listener_unsafe(type, listener);
    }

    void Manager::send_unsafe(const Type type, const Arg& arg)
    {
        const auto& v = listeners_[type];
        for (auto& l : v){
            l(arg);
        }
    }

    void Manager::send(const Type type, const Arg& arg)
    {
        std::shared_lock lock(mtx_);
        send_unsafe(type, arg);
    }

    void Manager::send_deferred(Type type, const Arg& arg, bool safe)
    {
        defer([type, safe, arg = Arg(arg)](Manager& m) mutable{
            safe ? m.send(type, arg) : m.send_unsafe(type, arg);
        });
    }

    void Manager::send_deferred(Type type, Arg&& arg, bool safe)
    {
        defer([type, safe, arg = Arg(std::forward<Arg>(arg))](Manager& m) mutable{
            safe ? m.send(type, arg) : m.send_unsafe(type, arg);
        });
    }
}
