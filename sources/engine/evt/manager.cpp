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

    Manager::~Manager()
    = default;

    ListenerHandle Manager::register_l_unsafe(const Type type, Listener listener)
    {
        auto& v = listeners_[type];
        v.emplace_back(std::move(listener));
        return v.size() - 1;
    }

    ListenerHandle Manager::register_l(const Type type, Listener listener)
    {
        std::unique_lock lock(mtx_);
        return register_l_unsafe(type, std::move(listener));
    }

    void Manager::unregister_l_unsafe(const Type type, const ListenerHandle listener)
    {
        auto& v = listeners_[type];
        if (listener >= v.size()) { return; }
        v[listener] = std::move(v.back());
        v.pop_back();
    }

    void Manager::unregister_l(const Type type, const ListenerHandle listener)
    {
        std::unique_lock lock(mtx_);
        unregister_l_unsafe(type, listener);
    }

    void Manager::send_unsafe(const Type type, const Arg& arg)
    {
        const auto& v = listeners_[type];
        for (auto& listener : v){
            listener(arg);
        }
    }

    void Manager::send(const Type type, const Arg& arg)
    {
        std::shared_lock lock(mtx_);
        send_unsafe(type, arg);
    }

    void Manager::send_deferred(const Type type, const Arg& arg, const bool safe)
    {
        deferred_actions_.emplace_back(
            [type, safe, arg = Arg(arg)](Manager& m) mutable{
                safe ? m.send(type, arg) : m.send_unsafe(type, arg);
            });
    }

    void Manager::send_deferred(Type type, Arg&& arg, const bool safe)
    {
        deferred_actions_.emplace_back(
            [type, safe, arg = Arg(std::forward<Arg>(arg))](Manager& m) mutable{
                safe ? m.send(type, arg) : m.send_unsafe(type, arg);
            });
    }

    void Manager::apply_deferred_actions()
    {
        for (auto& deferred_action : deferred_actions_){
            deferred_action(*this);
        }
        deferred_actions_.clear();
    }
}
