#include "pch.h"
#include <nasral/res/objects/resource.h>
#include <nasral/res/manager.h>

namespace nasral::res
{
    Resource::Resource(Manager* manager, const ResourceId& id, const Type type)
        : SubsystemObject(manager)
        , id_(id)
        , type_(type)
        , status_(Status::eUnloaded)
        , error_(Error::eNone)
    {
        if constexpr (kDebugBuild){
            auto* slot = subsystem()->slot(id_);
            assert(slot && "Resource slot is null");
            assert(slot->info.type == type_ && "Resource type mismatch");
        }
    }

    void Resource::set_status(const Status status){
        status_ = status;
    }

    void Resource::set_error(const Error error){
        error_ = error;
    }

    std::string Resource::id_str() const noexcept{
        return std::to_string(id_);
    }

    std::string Resource::status_str() const noexcept{
        return std::string(magic_enum::enum_name(status_));
    }

    std::string Resource::error_str() const noexcept{
        return std::string(magic_enum::enum_name(error_));
    }

    std::string Resource::type_str() const noexcept{
        return std::string(magic_enum::enum_name(type()));
    }
}
