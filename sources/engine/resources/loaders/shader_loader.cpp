#include "pch.h"
#include <nasral/resources/loaders/shader_loader.h>

namespace nasral::resources
{
    std::optional<Shader::Data> ShaderLoader::load(const std::string_view& path){
        std::ifstream file;
        file.open(path.data(), std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            err_code_ = ErrorCode::eCannotOpenFile;
            return std::nullopt;
        }

        const std::streamsize size = file.seekg(0, std::ios::end).tellg();
        if (size == 0 || size % 4 != 0) {
            err_code_ = ErrorCode::eBadFormat;
            return std::nullopt;
        }

        std::vector<std::uint32_t> shader_code(size / 4);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(shader_code.data()), size);
        file.close();

        err_code_ = ErrorCode::eNoError;
        return std::optional{Shader::Data{std::move(shader_code)}};
    }
}
