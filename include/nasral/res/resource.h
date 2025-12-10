#pragma once

#include <memory>
#include <nasral/res/types.h>
#include <nasral/log/loggable.h>
#include <magic_enum/magic_enum.hpp>

namespace nasral::res
{
    class Manager;
    class IResource
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<IResource> Ptr;
        virtual ~IResource() = default;
        virtual void load() noexcept = 0;

        [[nodiscard]] Type type() const noexcept { return type_; }
        [[nodiscard]] Status status() const noexcept { return status_; }
        [[nodiscard]] Error error() const noexcept { return error_; }
        [[nodiscard]] ResourceId id() const noexcept { return id_; }
        [[nodiscard]] Manager* manager() const noexcept { return manager_; }

        [[nodiscard]] std::string id_str() const noexcept{
            return std::to_string(id_);
        }

        [[nodiscard]] std::string type_str() const noexcept{
            return std::string(magic_enum::enum_name(type_));
        }

    protected:
        IResource(const Type type, const ResourceId id, Manager* manager)
            : type_(type)
            , status_(Status::eUnloaded)
            , error_(Error::eNone)
            , id_(id)
            , manager_(manager)
        {}

        Type type_;
        Status status_;
        Error error_;
        ResourceId id_;
        Manager* const manager_;
    };
}

namespace nasral::log
{
    class Logger;
    template <typename T>
    struct LoggerAccessor<T, std::enable_if_t<std::is_base_of_v<res::IResource, T>>> {
        static Logger* get(const T* resource) {
            return resource->manager()->engine()->logger();
        }
    };
}

#pragma region log_macros

#define RES_LOG_ERROR(error, msg) do { \
    const std::string err_type(magic_enum::enum_name(error)); \
    log_error("Resource [" + id_str() + "][" + type_str() + "] error (" + err_type + "). " + msg); \
} while (0)

#define RES_LOG_DESTRUCTION() log_info("Resource ["+id_str()+"]["+type_str()+"] destroyed.")

#define RES_LOG_LOADED() log_info("Resource ["+id_str()+"]["+type_str()+"] loaded.")

#pragma endregion