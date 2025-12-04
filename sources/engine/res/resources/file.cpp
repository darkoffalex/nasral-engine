#include "pch.h"
#include <nasral/res/resources/file.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    File::File(Manager* manager, ResourceId id)
        : IResource(Type::eFile, id, manager)
    {}

    File::~File(){
        if (file_.is_open()){
            file_.close();
        }

        log_info("Resource ["+id_str()+"]["+type_str()+"] destroyed.");
    }

    void File::load() noexcept{
        if (status_ == Status::eLoaded){
            return;
        }

        const auto path = manager()->path(id_, true);
        file_.open(path, std::ios::binary);
        if (!file_.is_open()){
            status_ = Status::eError;
            error_ = Error::eCannotOpenFile;
            log_error("Resource ["+std::to_string(id_)+"] error. Cannot open file: "+path);
            return;
        }

        status_ = Status::eLoaded;
        error_ = Error::eNone;
        log_info("Resource ["+std::to_string(id_)+"] loaded.");
    }

    bool File::read(void* buffer, const size_t size){
        if (status_ != Status::eLoaded){
            return false;
        }
        file_.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
        return file_.good();
    }
}
