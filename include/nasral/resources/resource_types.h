#pragma once
#include <string>

#define MAX_RESOURCE_COUNT 100
#define DEFAULT_REFS_COUNT 10
#define MAX_RESOURCE_PATH_LENGTH 64

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
        TOTAL
    };

    enum class Type : unsigned
    {
        eFile = 0,
        eTexture,
        eMesh,
        eShader,
        eShaderPipeline,
        TOTAL
    };

    class ResourceManager;
    class IResource
    {
    public:
        friend class ResourceManager;
        typedef std::unique_ptr<IResource> Ptr;
        virtual ~IResource() = default;
        virtual void load() = 0;

        [[nodiscard]] Status status() const { return status_; }
        [[nodiscard]] ErrorCode err_code() const { return err_code_; }
        [[nodiscard]] Type type() const { return type_; }

    protected:
        Type type_ = Type::eFile;
        Status status_ = Status::eUnloaded;
        ErrorCode err_code_ = ErrorCode::eNoError;
        ResourceManager* resource_manager_ = nullptr;
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
}
