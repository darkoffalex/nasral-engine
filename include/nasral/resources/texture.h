#pragma once
#include <memory>
#include <vector>
#include <vulkan/utils/image.hpp>
#include <nasral/resources/resource_types.h>

namespace nasral::resources
{
    class Texture final : public IResource
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

        explicit Texture(const ResourceManager* manager,
            const std::string_view& path,
            std::unique_ptr<Loader<Data>> loader);
        ~Texture() override;

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        void load() noexcept override;

        [[nodiscard]] const vk::Image& vk_image() const {return image_->image();}
        [[nodiscard]] const vk::ImageView& vk_image_view() const {return image_->image_view();}
        [[nodiscard]] const vk::DeviceMemory& vk_memory() const {return image_->memory();}
        [[nodiscard]] rendering::Handles::Texture render_handles() const;

    private:
        static vk::Format get_vk_format(uint32_t channels, uint32_t channel_depth);

    protected:
        std::string_view path_;
        std::unique_ptr<Loader<Data>> loader_;
        vk::utils::Image::Ptr image_;
    };
}