#pragma once
#include <filesystem>
#include <nasral/res/resources/texture.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace nasral::res
{
    class TextureStbLoader final : public Loader<Texture::Data>
    {
    public:
        explicit TextureStbLoader(const LoadParamsOpt& params = std::nullopt) : Loader(params)
        {
            if (params.has_value()){
                assert(std::holds_alternative<TextureLoadParams>(params.value()));
            }else{
                load_params_ = TextureLoadParams{};
            }
        }

        std::optional<Texture::Data> load(const std::string_view& path) override
        {
            if (!std::filesystem::exists(path)){
                error_ = Error::eCannotOpenFile;
                return std::nullopt;
            }

            int width = 0, height = 0, channels = 0;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* bytes = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);
            std::vector pixels(bytes, bytes + width * height * channels);
            stbi_image_free(bytes);

            error_ = Error::eNone;
            return std::optional{Texture::Data{
                std::move(pixels),
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                static_cast<uint32_t>(channels >= 3 ? 4 : channels),
                1
            }};
        }
    };
}