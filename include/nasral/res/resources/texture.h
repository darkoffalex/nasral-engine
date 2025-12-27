#pragma once

#include <vulkan/vulkan.hpp>
#include <nasral/res/resource.h>
#include <nasral/res/loader.h>
#include <nasral/log/loggable.h>
#include <vulkan/utils/image.hpp>
#include <nasral/gfx/types.h>

namespace nasral::res
{
    class Texture final : public IResource, public log::Loggable<Texture>
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

        Texture(Manager* manager, ResourceId id, Loader<Data>::Ptr loader);
        ~Texture() override;

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        void load() noexcept override;

        [[nodiscard]] const vk::Image& vk_image() const {return image_->image();}
        [[nodiscard]] const vk::ImageView& vk_image_view() const {return image_->image_view();}
        [[nodiscard]] const vk::DeviceMemory& vk_memory() const {return image_->memory();}
        [[nodiscard]] gfx::handles::Texture render_handles() const {return {vk_image_view()};}

    private:
        static vk::Format get_vk_format(uint32_t channels, uint32_t channel_depth, bool srgb = false);

    protected:
        Loader<Data>::Ptr loader_;
        vk::utils::Image::Ptr image_;
    };
}