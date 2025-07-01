#include "pch.h"
#include <nasral/resources/file.h>

#include "nasral/resources/resource_manager.h"

namespace nasral::resources
{
    File::File(ResourceManager* manager, const std::string_view& path)
        : path_(path)
    {
        assert(manager != nullptr);
        resource_manager_ = manager;
        status_ = Status::eUnloaded;
        err_code_ = ErrorCode::eNoError;
        type_ = Type::eFile;
    }

    File::~File(){
        if (file_.is_open()){
            file_.close();
        }
    }

    void File::load(){
        const auto path = resource_manager_->full_path(path_.data());
        file_.open(path, std::ios::binary);
        if (!file_.is_open()) {
            status_ = Status::eError;
            err_code_ = ErrorCode::eCannotOpenFile;
            const auto error_message = "Failed to open file: " + std::string(path_);
            throw std::runtime_error(error_message);
        }
        status_ = Status::eLoaded;
    }

    bool File::read(void* buffer, const size_t size){
        if (status_ != Status::eLoaded) return false;
        file_.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
        return file_.good();
    }
}
