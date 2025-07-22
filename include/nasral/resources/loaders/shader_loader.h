#pragma once
#include <nasral/resources/loaders/loader.h>
#include <nasral/resources/shader.h>

namespace nasral::resources
{
    class ShaderLoader final : public Loader<Shader::Data>
    {
    public:
        std::optional<Shader::Data> load(const std::string_view& path) override;
    };
}