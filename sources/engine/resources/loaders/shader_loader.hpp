#pragma once
#include <nasral/resources/shader.h>

namespace nasral::resources
{
    class ShaderLoader final : public Loader<Shader::Data>
    {
    public:
        std::optional<Shader::Data> load(const std::string_view& path) override{
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
    };
}