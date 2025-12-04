#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <array>
#include <stdexcept>

namespace nasral::res
{
    constexpr size_t kMaxResourcePathLength = 64;
    constexpr size_t kMaxResourceCount = 1024;
    constexpr size_t kMinRefsCount = 10;

    enum class Status : uint32_t
    {
        eUnloaded = 0,
        eLoaded,
        eError,
        TOTAL
    };

    enum class Error : uint32_t
    {
        eNone = 0,
        eLoadingFailed,
        eUnknownResource,
        eCannotOpenFile,
        eMemoryAllocationFailed,
        eVulkanError,
        eBadFormat,
        TOTAL
    };

    enum class Type : uint32_t
    {
        eFile = 0,
        eTexture,
        eMesh,
        eShader,
        eMaterial,
        TOTAL
    };

    constexpr std::string_view kBuiltinTexWhitePixel    = "builtin:tex/white-pixel";
    constexpr std::string_view kBuiltinTexBlackPixel    = "builtin:tex/black-pixel";
    constexpr std::string_view kBuiltinTexNormPixel     = "builtin:tex/normal-pixel";
    constexpr std::string_view kBuiltinCheckerboard     = "builtin:tex/chessboard-64-16";
    constexpr std::string_view kBuiltinMeshQuad         = "builtin:mesh/quad";
    constexpr std::string_view kBuiltinMeshCube         = "builtin:mesh/cube";
    constexpr std::string_view kBuiltinMeshSphere       = "builtin:mesh/sphere";

    struct TextureLoadParams
    {
        bool srgb = false;
        bool generate_mipmaps = true;
    };

    struct MeshLoadParams
    {
        bool generate_tangents = false;
        bool generate_normals = false;
        bool winding_order_ccw = false;
    };

    using LoadParams = std::variant<TextureLoadParams, MeshLoadParams>;
    using LoadParamsOpt = std::optional<LoadParams>;

    struct Path
    {
        using Buffer = std::array<char, kMaxResourcePathLength>;
        Buffer buffer{};

        Path(){ buffer[0] = '\0'; }
        explicit Path(const std::string& path);

        void assign(const std::string& path){
            if (path.size() > kMaxResourcePathLength - 1){
                throw std::runtime_error("Path string is too long");
            }

            const auto size = std::min(path.size(), buffer.size() - 1);
            std::copy_n(path.cbegin(), size, buffer.begin());
            buffer[size] = '\0';
        }

        [[nodiscard]] std::string_view view() const{
            return {buffer.data()};
        }

        [[nodiscard]] const char* data() const{
            return buffer.data();
        }

        bool operator==(const Path& other) const{
            return view() == other.view();
        }
    };

    using ResourceId = uint32_t;

    struct Config
    {
        std::string content_dir;
        std::vector<std::tuple<Type, std::string, LoadParamsOpt>> initial_resources;
    };
}
