#pragma once
#include <nasral/resources/texture.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace nasral::resources
{
    class TextureLoader final : public Loader<Texture::Data>
    {
    public:
        explicit TextureLoader(const std::optional<LoadParams>& params = std::nullopt) : Loader(params){
            if (load_params_.has_value()){
                assert(std::get_if<TextureLoadParams>(&load_params_.value()) != nullptr);
            }else{
                load_params_ = TextureLoadParams();
            }
        }

        std::optional<Texture::Data> load([[maybe_unused]] const std::string_view& path) override{
            if (!std::filesystem::exists(path)){
                return std::nullopt;
            }

            int width = 0, height = 0, channels = 0;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* bytes = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);
            std::vector pixels(bytes, bytes + width * height * channels);
            stbi_image_free(bytes);

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