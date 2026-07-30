#pragma once

#include <vector>
#include <filesystem>
#include <nasral/res/objects/texture.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace nasral::res
{
    class TextureStbLoader final : public Texture::Loader<Texture::Data>
    {
    public:
        explicit TextureStbLoader(Manager* manager, const std::optional<LoadParams>& params = std::nullopt)
            : Loader(manager, params)
        {
            if (params.has_value()){
                assert(std::holds_alternative<TextureLoadParams>(params.value()) && "Wrong loading params");
            }else{
                set_params(TextureLoadParams{false, true});
            }
        }

        std::optional<Texture::Data> load(const std::string_view& file_path) override
        {
            if (!std::filesystem::exists(file_path)){
                set_error(Error::eCannotOpenFile);
                return std::nullopt;
            }

            int width = 0, height = 0, channels = 0;
            stbi_set_flip_vertically_on_load(true);

            // Узнаем информацию об изображении до распаковки
            if (!stbi_info(file_path.data(), &width, &height, &channels)) {
                set_error(Error::eCannotOpenFile);
                return std::nullopt;
            }

            // Vulkan часто не поддерживает 24-битные форматы (3 канала),
            // поэтому принудительно расширяем RGB до RGBA.
            // Одноканальные и двухканальные текстуры оставляем как есть.
            const int desired_channels = (channels == 3) ? 4 : channels;

            // Загружаем с нужным количеством каналов
            unsigned char* bytes = stbi_load(file_path.data(), &width, &height, &channels, desired_channels);
            if (!bytes) {
                set_error(Error::eCannotOpenFile);
                return std::nullopt;
            }

            // Вектор должен использовать desired_channels, т.к. именно такой размер вернул stbi_load
            std::vector pixels(bytes, bytes + width * height * desired_channels);
            stbi_image_free(bytes);

            set_error(Error::eNone);
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