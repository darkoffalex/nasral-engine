#pragma once
#include <nasral/res/loader.h>
#include <nasral/res/resources/shader.h>

namespace nasral::res
{
    class ShaderSpvLoader final : public Loader<Shader::Data>
    {
    public:
        explicit ShaderSpvLoader(Engine* const engine) : Loader(engine)
        {}

        std::optional<Shader::Data> load(const std::string_view& path) override{
            std::ifstream file{path.data(), std::ios::binary | std::ios::ate};

            if (!file.is_open()){
                error_ = Error::eCannotOpenFile;
                return std::nullopt;
            }

            const auto size = static_cast<std::size_t>(file.seekg(0, std::ios::end).tellg());
            if (size == 0 || size % sizeof(std::uint32_t) != 0){
                error_ = Error::eBadFormat;
                return std::nullopt;
            }

            std::vector<std::uint32_t> shader_code(size / 4);
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(shader_code.data()), size);
            file.close();

            error_ = Error::eNone;
            return std::optional{Shader::Data{shader_code}};
        }
    };
}