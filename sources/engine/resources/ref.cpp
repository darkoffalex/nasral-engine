#include "pch.h"
#include <nasral/resources/ref.h>
#include <nasral/resources/resource_manager.h>

#include "nasral/logging/logger.h"

namespace nasral::resources
{
    Ref::Ref()
        : type_(Type::eFile)
        , resource_index_(std::nullopt)
        , is_requested_(false)
        , is_handled_(false)
        , on_ready_(nullptr)
    {}

    Ref::Ref(ResourceManager* manager, const Type type, const std::string& path)
        : type_(type)
        , resource_index_(std::nullopt)
        , is_requested_(false)
        , is_handled_(false)
        , manager_(manager)
        , on_ready_(nullptr)
    {
        path_.assign(path);
    }

    Ref::Ref(const Ref& other)
        : type_(other.type_)
        , resource_index_(other.resource_index_)
        , is_requested_(false)
        , is_handled_(false)
        , manager_(other.manager_)
        , on_ready_(nullptr)
    {
        path_ = other.path_;
    }

    Ref& Ref::operator=(const Ref& other){
        if (this == &other) return *this;

        release();
        type_ = other.type_;
        resource_index_ = other.resource_index_;
        is_requested_ = false;
        is_handled_ = false;
        manager_ = other.manager_;
        on_ready_ = nullptr;

        return *this;
    }

    Ref::~Ref(){
       release();
    }

    void Ref::request(){
        if (is_requested_) {
            const auto msg = "Attempt to request already requested resource (" + std::string(path_.data()) + ")";
            manager_->logger()->warning(msg);
            return;
        }

        manager_->request(this);
        is_requested_ = true;
    }

    void Ref::release(){
        if (!is_requested_) {
            return;
        }

        manager_->release(this);
        is_requested_ = false;
    }

    void Ref::set_path(const std::string& path){
        if (is_requested_) {
            const auto msg = "Attempt to set path for already requested resource (" + std::string(path_.data()) + ")";
            manager_->logger()->warning(msg);
            return;
        }

        path_.assign(path);
    }

    void Ref::set_callback(std::function<void(IResource*)> callback){
        if (is_requested_) {
            const auto msg = "Attempt to set callback for already requested resource (" + std::string(path_.data()) + ")";
            manager_->logger()->warning(msg);
            return;
        }

        on_ready_ = std::move(callback);
    }

    const IResource* Ref::resource() const{
        if (!is_requested_ || !resource_index_.has_value()){
            manager_->logger()->warning("Attempt to access resource before request (" + std::string(path_.data()) + ")");
            return nullptr;
        }

        return manager_->get_resource(resource_index_.value());
    }
}
