#include "pch.h"
#include <nasral/engine.h>
#include <nasral/resources/file.h>

namespace nasral::resources
{
    File::File(const ResourceManager* manager, const std::string_view& path)
        : IResource(Type::eFile, manager, manager->engine()->logger())
        , path_(path)
    {}

    File::~File(){
        if (file_.is_open()){
            file_.close();
        }
    }

    void File::load() noexcept{
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->full_path(path_.data());
        file_.open(path, std::ios::binary);
        if (!file_.is_open()) {
            status_ = Status::eError;
            err_code_ = ErrorCode::eCannotOpenFile;
            logger()->error("Can't open file: " + path);
            return;
        }
        status_ = Status::eLoaded;
    }

    bool File::read(void* buffer, const size_t size){
        if (status_ != Status::eLoaded) return false;
        file_.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
        return file_.good();
    }
}
