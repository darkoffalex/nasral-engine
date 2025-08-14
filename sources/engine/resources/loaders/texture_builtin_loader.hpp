#pragma once
#include <nasral/resources/texture.h>

namespace nasral::resources
{
    class TextureBuiltinLoader final : public Loader<Texture::Data>
    {
    public:
        std::optional<Texture::Data> load([[maybe_unused]] const std::string_view& path) override{
            const auto wp_name = name_of(BuiltinResources::eWhitePixel, kBuiltinResources);
            const auto bp_name = name_of(BuiltinResources::eBlackPixel, kBuiltinResources);
            const auto np_name = name_of(BuiltinResources::eNormalPixel, kBuiltinResources);
            const auto cb_name = name_of(BuiltinResources::eCheckerboardTexture, kBuiltinResources);

            if (path.find(wp_name) != std::string_view::npos){
                return std::optional{Texture::Data{
                    {255, 255, 255, 255},
                    1, 1, 4, 1
                }};
            }
            if (path.find(bp_name) != std::string_view::npos){
                return std::optional{Texture::Data{
                    {0, 0, 0, 255},
                    1, 1, 4, 1
                }};
            }
            if (path.find(np_name) != std::string_view::npos){
                return std::optional{Texture::Data{
                    {128, 128, 255, 255},
                    1, 1, 4, 1
                }};
            }
            if (path.find(cb_name) != std::string_view::npos){
                std::vector<unsigned char> pixels(64 * 64 * 4);
                for (int y = 0; y < 64; ++y) {
                    for (int x = 0; x < 64; ++x) {
                        const int square_x = x / 16;
                        const int square_y = y / 16;
                        const unsigned char color = ((square_x + square_y) % 2 == 0) ? 255 : 0;
                        const int index = (y * 64 + x) * 4;
                        pixels[index + 0] = color; // R
                        pixels[index + 1] = color; // G
                        pixels[index + 2] = color; // B
                        pixels[index + 3] = 255;   // A
                    }
                }
                return std::optional{Texture::Data{
                    std::move(pixels),
                    64, 64, 4, 1
                }};
            }
            return std::nullopt;
        }
    };
}