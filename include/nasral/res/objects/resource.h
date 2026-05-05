#pragma once

#include <memory>
#include <nasral/res/types.h>
#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>

namespace nasral::res
{
    class Manager;
    class Resource : public SubsystemObject<Manager>, public log::Loggable<Resource>
    {
    public:
        friend class Manager;
        typedef std::unique_ptr<Resource> Ptr;

        template<typename Data>
        class Loader : public SubsystemObject
        {
        public:
            typedef std::unique_ptr<Loader> Ptr;
            explicit Loader(Manager* manager, const std::optional<LoadParams>& params = std::nullopt)
                : SubsystemObject(manager)
                , params_(params)
            {
                static_assert(std::is_default_constructible_v<Data>, "Data must be default constructible");
                static_assert(std::is_move_assignable_v<Data>, "Data must be move assignable");
            }
            virtual ~Loader() = default;
            virtual std::optional<Data> load(const std::string_view& file_path) = 0;
            [[nodiscard]] Error error() const noexcept { return error_; }

            template<typename LP>
            [[nodiscard]] const LP* params() const noexcept {
                return params_.has_value() ? std::get_if<LP>(&params_.value()) : nullptr;
            }

        protected:
            void set_error(const Error error) { error_ = error; }

        private:
            Error error_ = Error::eNone;
            std::optional<LoadParams> params_ = std::nullopt;
        };

        virtual ~Resource() = default;
        virtual void load() noexcept = 0;

        [[nodiscard]] ResourceId id() const noexcept { return id_; }
        [[nodiscard]] Status status() const noexcept { return status_; }
        [[nodiscard]] Error error() const noexcept { return error_; }
        [[nodiscard]] Type type() const noexcept {return type_;}

        [[nodiscard]] std::string id_str() const noexcept;
        [[nodiscard]] std::string status_str() const noexcept;
        [[nodiscard]] std::string error_str() const noexcept;
        [[nodiscard]] std::string type_str() const noexcept;

        Resource(const Resource&) = delete;
        Resource& operator=(const Resource&) = delete;

    protected:
        Resource(Manager* manager, const ResourceId& id, Type type);
        void set_status(Status status);
        void set_error(Error error);

    private:
        ResourceId id_;
        Type type_;
        Status status_;
        Error error_;
    };
}

DECLARE_SUBSYSTEM_OBJ_LOGGER_ACCESSOR(res::Resource)

#define RES_LOG_ERROR(error, msg) do { \
    log_error("Resource [" + id_str() + "][" + type_str() + "] error (" + error_str() + "). " + msg); \
} while (0)

#define RES_LOG_LOADED() do { \
    const std::string p = subsystem()->path(id(), false); \
    log_info("Resource ["+id_str()+"|"+p+"]["+type_str()+"] loaded."); \
} while (0)

#define RES_LOG_DESTRUCTION() do { \
    const std::string p = subsystem()->path(id(), false); \
    log_info("Resource ["+id_str()+"|"+p+"]["+type_str()+"] destroyed."); \
} while (0)
