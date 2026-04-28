#pragma once

#include <memory>
#include <nasral/evt/types.h>
#include <nasral/common/subsystem.h>

namespace nasral::evt
{
    class Manager;
    class Listener : public SubsystemObject<Manager>
    {
    public:
        typedef std::unique_ptr<Listener> Ptr;

        Listener(Manager* manager, Type type, ListenerCallback&& callback, bool safe = true);
        ~Listener();

        Listener(const Listener&) = delete;
        Listener& operator=(const Listener&) = delete;

        static Ptr reg(Manager* manager
            , Type type
            , ListenerCallback&& callback
            , bool safe = true);

    private:
        bool safe_;
        Type type_;
        ListenerHandle handle_;
    };
}