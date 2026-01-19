#include "pch.h"
#include <nasral/res/resources/texture.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Texture::Texture(Manager* manager, const ResourceId id, Loader<Data>::Ptr loader)
        : IResource(Type::eTexture, id, manager)
        , loader_(std::move(loader))
    {}

    Texture::~Texture(){
        image_.reset();
        RES_LOG_DESTRUCTION();
    }

    void Texture::load() noexcept
    {
        assert(loader_ != nullptr);
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->path(id_, true);
        std::optional<Data> data = std::nullopt;

        try
        {
            data = loader_->load(path);
            if (!data.has_value()){
                status_ = Status::eError;
                error_ = loader_->error();
                throw std::runtime_error("Failed to load texture:" + path);
            }
        }
        catch (const std::exception& e)
        {
            status_ = Status::eError;
            error_ = error_ == Error::eNone ? Error::eLoadingFailed : error_;
            RES_LOG_ERROR(error_, e.what());
            return;
        }

        try
        {
            constexpr auto group_idx = static_cast<size_t>(gfx::Renderer::CommandGroup::eGraphicsAndPresent);
            const auto renderer = manager()->engine()->renderer();
            auto& cmd_group = renderer->vk_device().queue_group(group_idx);
            const auto* lp = loader_->load_params<TextureLoadParams>();

            // Получить формат в зависимости от кол-ва байт на пиксель
            const vk::Format desired_format = get_vk_format(data->channels, data->channel_depth, lp ? lp->srgb : false);
            assert(desired_format != vk::Format::eUndefined);

            // Проверить доступность формата
            const auto fp = renderer->vk_device().physical_device().getFormatProperties(desired_format);
            if (!(fp.linearTilingFeatures & vk::FormatFeatureFlagBits::eTransferSrc))
            {
                status_ = Status::eError;
                error_ = Error::eVulkanError;
                RES_LOG_ERROR(error_, "Format not supported for linear tiling and transfer src: " + path);
                return;
            }
            if (!(fp.optimalTilingFeatures &
                (vk::FormatFeatureFlagBits::eSampledImage |
                vk::FormatFeatureFlagBits::eTransferDst |
                vk::FormatFeatureFlagBits::eTransferSrc)))
            {
                status_ = Status::eError;
                error_ = Error::eVulkanError;
                RES_LOG_ERROR(error_, "Format not supported for optimal tiling and transfer dst/src: " + path);
                return;
            }

            // Создать временное изображение
            const auto staging_image = std::make_unique<vk::utils::Image>(renderer->vk_device_ptr()
                , vk::utils::Image::Type::e2D
                , desired_format
                , vk::Extent3D{data->width, data->height, 1}
                , vk::ImageUsageFlagBits::eTransferSrc
                , vk::ImageTiling::eLinear
                , vk::ImageAspectFlagBits::eColor
                , vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
                , vk::ImageLayout::ePreinitialized
                , vk::SampleCountFlagBits::e1
                , 1   // В промежуточном изображении не нужны мип-уровни
                , 1); // Кол-вл слоев (1 слой - обычная текстура)

            // Создать целевое изображение
            image_ = std::make_unique<vk::utils::Image>(renderer->vk_device_ptr()
                , vk::utils::Image::Type::e2D
                , desired_format
                , vk::Extent3D{data->width, data->height, 1}
                , vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled
                , vk::ImageTiling::eOptimal
                , vk::ImageAspectFlagBits::eColor
                , vk::MemoryPropertyFlagBits::eDeviceLocal
                , vk::ImageLayout::ePreinitialized
                , vk::SampleCountFlagBits::e1
                , 0   // Автоматически создать мип-уровни
                , 1); // Кол-вл слоев (1 слой - обычная текстура)

            // Копировать данные (загруженные пиксели) в промежуточное изображение
            auto* mem = static_cast<uint8_t*>(staging_image->map(vk::ImageAspectFlagBits::eColor));
            const auto isl = renderer->vk_device().logical_device().getImageSubresourceLayout(
                staging_image->image(),
                {vk::ImageAspectFlagBits::eColor, 0, 0});

            if (isl.rowPitch == data->width * (data->channels * data->channel_depth)){
                memcpy(mem, data->pixels.data(), data->width * data->height * (data->channels * data->channel_depth));
            }else{
                for (std::size_t y = 0; y < data->height; ++y){
                    memcpy(mem + isl.offset + y * isl.rowPitch
                        , data->pixels.data() + y * data->width * (data->channels * data->channel_depth)
                        , data->width * (data->channels * data->channel_depth));
                }
            }

            staging_image->unmap();

            // Копировать данные из временного изображения в целевое
            staging_image->copy_to(*image_
                , cmd_group
                , vk::Extent3D{data->width, data->height, 1}
                , vk::ImageAspectFlagBits::eColor
                , vk::ImageAspectFlagBits::eColor
                , 1
                , 1
                , true);

            // Генерация мип-уровней
            if (image_->mip_levels() > 1 && (lp ? lp->generate_mipmaps : false)){
                image_->generate_mipmaps(cmd_group
                    , vk::Extent3D{data->width, data->height, 1}
                    , vk::ImageAspectFlagBits::eColor
                    , 1);
            }
        }
        catch (const std::exception& e)
        {
            status_ = Status::eError;
            error_ = Error::eVulkanError;
            RES_LOG_ERROR(Error::eLoadingFailed, e.what());
            return;
        }

        status_ = Status::eLoaded;
        error_ = Error::eNone;
        RES_LOG_LOADED();
    }

    vk::Format Texture::get_vk_format(const uint32_t channels, const uint32_t channel_depth, const bool srgb)
    {
        // 1 байт (8 бит) на канал
        if (channel_depth == 1) {
            switch (channels) {
            case 1: return srgb ? vk::Format::eR8Srgb : vk::Format::eR8Unorm;
            case 2: return srgb ? vk::Format::eR8G8Srgb : vk::Format::eR8G8Unorm;
            case 3: return srgb ? vk::Format::eR8G8B8Srgb : vk::Format::eR8G8B8Unorm;
            case 4: return srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
            default: return vk::Format::eUndefined;
            }
        }
        // 2 байта (16 бит) на канал
        if (channel_depth == 2) { // 16-bit per channel
            switch (channels) {
            case 1: return vk::Format::eR16Unorm;
            case 2: return vk::Format::eR16G16Unorm;
            case 3: return vk::Format::eR16G16B16Unorm;
            case 4: return vk::Format::eR16G16B16A16Unorm;
            default: return vk::Format::eUndefined;
            }
        }
        // 4 байта (32 бит) на канал (HDR текстуры)
        if (channel_depth == 4) {
            switch (channels) {
            case 1: return vk::Format::eR32Sfloat;
            case 2: return vk::Format::eR32G32Sfloat;
            case 3: return vk::Format::eR32G32B32Sfloat;
            case 4: return vk::Format::eR32G32B32A32Sfloat;
            default: return vk::Format::eUndefined;
            }
        }
        return vk::Format::eUndefined;
    }
}
