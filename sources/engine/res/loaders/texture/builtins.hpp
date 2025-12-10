#pragma once
#include <nasral/res/resources/texture.h>

namespace nasral::res
{
    class TextureBuiltinLoader final : public Loader<Texture::Data>
    {
    public:
        std::optional<Texture::Data> load(const std::string_view& path) override
        {
            if (path.find(kBuiltinTexWhitePixel) != std::string::npos){
                return std::optional{Texture::Data{
                        {255, 255, 255, 255},
                        1, 1, 4, 1
                }};
            }
            if (path.find(kBuiltinTexBlackPixel) != std::string::npos){
                return std::optional{Texture::Data{
                        {0, 0, 0, 255},
                        1, 1, 4, 1
                }};
            }
            if (path.find(kBuiltinTexNormPixel) != std::string::npos){
                return std::optional{Texture::Data{
                        {128, 128, 128, 255},
                        1, 1, 4, 1
                }};
            }
            if (path.find(kBuiltinCheckerboard) != std::string::npos){
                constexpr int size = 64;
                std::vector<unsigned char> pixels(size * size * 4);
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        constexpr int cell = 16;
                        const int square_x = x / cell;
                        const int square_y = y / cell;
                        const unsigned char color = ((square_x + square_y) % 2 == 0) ? 255 : 0;
                        const int index = (y * size + x) * 4;
                        pixels[index + 0] = color; // R
                        pixels[index + 1] = color; // G
                        pixels[index + 2] = color; // B
                        pixels[index + 3] = 255;   // A
                    }
                }
                return std::optional{Texture::Data{
                    std::move(pixels),
                    size, size, 4, 1
                }};
            }
            return std::nullopt;
        }
    };
}