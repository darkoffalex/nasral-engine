#include "pch.h"
#include <nasral/resources/request.h>
#include <nasral/resources/resource_manager.h>

namespace nasral::resources
{
    Request::Request(ResourceManager* manager, const std::string& path, RequestCallback on_ready)
    : manager_(manager)
    {
        // Если путь пуст
        if (path.empty()){
            throw ResourceError("Empty resource path");
        }

        // Получить string_view ссылающийся на централизованный список ресурсов
        const auto p = manager_->res_path(path);
        // Если ресурса нет в списке
        if (!p.has_value()){
            throw ResourceError("Resource not found in the list (" + path + ")");
        }

        path_ = p.value();
        id_ = manager_->request(path, std::move(on_ready));
    }

    Request::~Request(){
        manager_->release(path_, id_);
    }

    bool Request::is_requested() const noexcept{
        return id_.has_value();
    }

    bool Request::is_unhandled() const noexcept{
        if (!id_.has_value()){
            return false;
        }
        return manager_->is_unhandled(path_, id_.value());
    }
}
