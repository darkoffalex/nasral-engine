#include "pch.h"
#include <nasral/res/objects/file.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    File::File(Manager* manager, const ResourceId& id)
        : Resource(manager, id, Type::eFile)
    {}

    File::~File(){
        if (file_.is_open()){
            file_.close();
        }
        RES_LOG_DESTRUCTION();
    }

    void File::load() noexcept{
        if (status() == Status::eLoaded){
            return;
        }

        const auto path = subsystem()->path(id(), true);
        file_.open(path, std::ios::binary);
        if (!file_.is_open()){
            set_status(Status::eError);
            set_error(Error::eCannotOpenFile);
            RES_LOG_ERROR(error_, "Failed to open file:" + path);
            return;
        }

        set_status(Status::eLoaded);
        set_error(Error::eNone);
        RES_LOG_LOADED();
    }

    bool File::read(void* buffer, const size_t size){
        if (status() != Status::eLoaded){
            return false;
        }
        file_.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
        return file_.good();
    }
}
