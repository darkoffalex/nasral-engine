#pragma once
#include <string>
#include <array>
#include <variant>
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

    inline const std::array<std::string, static_cast<size_t>(Status::TOTAL)> kStatusNames = {
        "Unloaded",
        "Loaded",
        "Error"
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

    inline const std::array<std::string, static_cast<size_t>(ErrorCode::TOTAL)> kErrorNames = {
        "No error",
        "Loading error",
        "Unknown resource",
        "Cannot open file",
        "Memory allocation failed",
        "Vulkan error",
        "Bad format"
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

    inline const std::array<std::string, static_cast<size_t>(Type::TOTAL)> kTypeNames = {
        "File",
        "Texture",
        "Mesh",
        "Shader",
        "Material"
    };

    enum class BuiltinResources : unsigned
    {
        eWhitePixel = 0,
        eBlackPixel,
        eNormalPixel,
        eCheckerboardTexture,
        eQuadMesh,
        eCubeMesh,
        eSphereMesh,
        TOTAL
    };

    inline const std::array<std::string, static_cast<size_t>(BuiltinResources::TOTAL)> kBuiltinResources = {
        "builtin:tex/white-pixel",
        "builtin:tex/black-pixel",
        "builtin:tex/normal-pixel",
        "builtin:tex/chessboard-64-16",
        "builtin:mesh/quad",
        "builtin:mesh/cube",
        "builtin:mesh/sphere"
    };

    inline std::string builtin_res_path(const BuiltinResources res){
        return name_of(res, kBuiltinResources);
    }

    inline Type builtin_res_type(const std::string& path){
        auto type = Type::TOTAL;
        if (path.find("builtin:tex") != std::string_view::npos){
            type = Type::eTexture;
        }else if (path.find("builtin:mesh") != std::string_view::npos){
            type = Type::eMesh;
        }else if (path.find("builtin:shd") != std::string_view::npos){
            type = Type::eShader;
        }else if (path.find("builtin:mtl") != std::string_view::npos){
            type = Type::eMaterial;
        }
        return type;
    }

    inline Type builtin_res_type(const BuiltinResources res){
        const auto path = name_of(res, kBuiltinResources);
        return builtin_res_type(path);
    }

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

    struct TextureLoadParams
    {
        bool srgb = false;
        bool gen_mipmaps = true;

        TextureLoadParams& set_srgb(const bool i_srgb){
            this->srgb = i_srgb;
            return *this;
        }

        TextureLoadParams& set_gen_mipmaps(const bool i_gen_mipmaps){
            this->gen_mipmaps = i_gen_mipmaps;
            return *this;
        }
    };

    struct MeshLoadParams
    {
        bool gen_normals = true;
        bool gen_tangents = false;
        bool winding_ccw = false;

        MeshLoadParams& set_gen_normals(const bool i_gen_normals){
            this->gen_normals = i_gen_normals;
            return *this;
        }

        MeshLoadParams& set_gen_tangents(const bool i_gen_tangents){
            this->gen_tangents = i_gen_tangents;
            return *this;
        }

        MeshLoadParams& set_winding_ccw(const bool i_winding_ccw){
            this->winding_ccw = i_winding_ccw;
            return *this;
        }
    };

    using LoadParams = std::variant<TextureLoadParams, MeshLoadParams>;

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
        std::vector<std::tuple<Type, std::string, std::optional<LoadParams>>> initial_resources;
    };

    class ResourceError final : public EngineError
    {
    public:
        explicit ResourceError(const std::string& message)
        : EngineError(message) {}
    };
}
