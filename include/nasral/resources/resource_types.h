#pragma once
#include <string>
#include <array>
#include <nasral/core_types.h>

#define MAX_RESOURCE_COUNT 100
#define DEFAULT_REFS_COUNT 10
#define MAX_RESOURCE_PATH_LENGTH 64

namespace nasral::logging
{
    class Logger;
}

namespace nasral::resources
{
    enum class Status : unsigned
    {
        eUnloaded = 0,
        eLoaded,
        eError,
        TOTAL
    };

    enum class ErrorCode : unsigned
    {
        eNoError = 0,
        eLoadingError,
        eUnknownResource,
        eCannotOpenFile,
        eMemoryAllocationFailed,
        eVulkanError,
        eBadFormat,
        TOTAL
    };

    enum class Type : unsigned
    {
        eFile = 0,
        eTexture,
        eMesh,
        eShader,
        eMaterial,
        TOTAL
    };

    class ResourceManager;
    class IResource
    {
    public:
        friend class ResourceManager;
        typedef std::unique_ptr<IResource> Ptr;
        virtual ~IResource() = default;
        virtual void load() noexcept = 0;

        [[nodiscard]] Status status() const { return status_; }
        [[nodiscard]] ErrorCode err_code() const { return err_code_; }
        [[nodiscard]] Type type() const { return type_; }
        [[nodiscard]] const SafeHandle<const ResourceManager>& manager() const { return resource_manager_; }
        [[nodiscard]] const SafeHandle<const logging::Logger>& logger() const { return logger_; }

    protected:
        IResource(const Type type, const ResourceManager* manager, const logging::Logger* logger)
            : type_(type)
            , resource_manager_(manager)
            , logger_(logger)
        {}

        Type type_ = Type::eFile;
        Status status_ = Status::eUnloaded;
        ErrorCode err_code_ = ErrorCode::eNoError;
        SafeHandle<const ResourceManager> resource_manager_;
        SafeHandle<const logging::Logger> logger_;
    };

    struct FixedPath
    {
        using Buff = std::array<char, MAX_RESOURCE_PATH_LENGTH>;
        Buff buff{};

        FixedPath(){
            buff[0] = '\0';
        }

        explicit FixedPath(const std::string& path){
            assign(path);
        }

        void assign(const std::string& path){
            if (path.size() > MAX_RESOURCE_PATH_LENGTH){
                throw std::invalid_argument("Path string is too long");
            }

            const auto len = std::min(path.size(), buff.size() - 1);
            std::copy_n(path.begin(), len, buff.begin());
            buff[len] = '\0';
        }

        [[nodiscard]] std::string_view view() const {
            return {buff.data()};
        }

        [[nodiscard]] const char* data() const {
            return buff.data();
        }

        bool operator==(const FixedPath& other) const {
            return view() == other.view();
        }
    };

    template<typename DataType>
    class Loader
    {
        static_assert(std::is_move_constructible_v<DataType>, "DataType must be move constructible");
        static_assert(std::is_move_assignable_v<DataType>, "DataType must be move assignable");

    public:
        virtual std::optional<DataType> load(const std::string_view& path) = 0;
        virtual ~Loader() = default;

        [[nodiscard]] ErrorCode err_code() const { return err_code_; }

    protected:
        ErrorCode err_code_ = ErrorCode::eNoError;
    };

    struct ResourceConfig
    {
        std::string content_dir;
        std::vector<std::pair<Type, std::string>> initial_resources;
    };

    class ResourceError final : public EngineError
    {
    public:
        explicit ResourceError(const std::string& message)
        : EngineError(message) {}
    };
}
