#pragma once

#include <nasral/res/objects/resource.h>
#include <vulkan/utils/image.hpp>
#include <nasral/gfx/types.h>

namespace nasral::res
{
    class Texture final : public Resource
    {
    public:
        typedef std::unique_ptr<Texture> Ptr;

        struct Data
        {
            std::vector<unsigned char> pixels = {};
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t channels = 0;
            uint32_t channel_depth = 1;
        };

        Texture(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader);
        ~Texture() override;

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        void load() noexcept override;

        [[nodiscard]] const auto& vk_image() const noexcept { return image_; }
        [[nodiscard]] auto render_handles() const {return gfx::handles::Texture{image_->image_view()};}

    private:
        Loader<Data>::Ptr loader_;
        vk::utils::Image::Ptr image_;
    };
}
