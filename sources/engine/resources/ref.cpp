#include "pch.h"
#include <nasral/resources/ref.h>
#include <nasral/resources/resource_manager.h>

namespace nasral::resources
{
    Ref::Ref(ResourceManager* manager, const Type type, const std::string& path)
        : type_(type)
        , resource_index_(std::nullopt)
        , is_requested_(false)
        , manager_(manager)
        , on_ready_(nullptr)
    {
        if (manager_ == nullptr){
            throw std::invalid_argument("Resource manager is null");
        }

        path_.assign(path);
    }

    Ref::~Ref(){
       release();
    }

    void Ref::request(){
        if (is_requested_) return;
        manager_->request(this);
        is_requested_ = true;
    }

    void Ref::release(){
        if (!is_requested_) return;
        manager_->release(this);
        is_requested_ = false;
    }

    void Ref::set_callback(const std::function<void(IResource*)>& callback){
        on_ready_ = callback;
    }
}
