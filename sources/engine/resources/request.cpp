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
        id_ = manager_->request(path_, std::move(on_ready));
    }

    Request::Request(ResourceManager* manager, const std::string_view& path, RequestCallback on_ready)
    : path_(path)
    , manager_(manager)
    {
        // Корректный путь (string_view) должен быть передан при использовании конструктора
        if (path_.empty()){
            throw ResourceError("Empty resource path");
        }

        id_ = manager_->request(path_, std::move(on_ready));
    }

    Request::~Request(){
        if (!id_.has_value()) return;
        manager_->release(path_, id_);
    }

    Request::Request(Request&& other) noexcept
        : id_(std::exchange(other.id_, std::nullopt))
        , path_(std::exchange(other.path_, ""))
        , manager_(std::exchange(other.manager_,  {}))
    {}

    Request& Request::operator=(Request&& other) noexcept{
        if (this == &other) return *this;
        id_ = std::exchange(other.id_, std::nullopt);
        path_ = std::exchange(other.path_, "");
        manager_ = std::exchange(other.manager_,  {});
        return *this;
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
