#include "pch.h"
#include <nasral/evt/manager.h>
#include <nasral/evt/objects/listener.h>

namespace nasral::evt
{
    Listener::Listener(Manager* manager, const Type type, ListenerCallback&& callback, const bool safe)
        : SubsystemObject(manager)
        , safe_(safe)
        , type_(type)
        , handle_(kInvalidListener)
    {
        assert(manager && "Manager cannot be null");  // Или throw
        handle_ = safe_ ?
            subsystem()->register_listener(type, std::move(callback)) :
            subsystem()->register_listener_unsafe(type, std::move(callback));
    }

   Listener::~Listener()
   {
       if (subsystem()) {
           safe_ ?
               subsystem()->unregister_listener(type_, handle_) :
               subsystem()->unregister_listener_unsafe(type_, handle_);
       }
   }

   Listener::Ptr Listener::reg(Manager* manager, Type type, ListenerCallback&& callback, bool safe){
        return std::make_unique<Listener>(manager, type, std::move(callback), safe);
   }
}
