#include "pch.h"
#include <nasral/engine.h>
#include <nasral/resources/texture.h>

namespace nasral::resources
{
    Texture::Texture(const ResourceManager* manager, const std::string_view& path, std::unique_ptr<Loader<Data>> loader)
        : IResource(Type::eShader, manager, manager->engine()->logger())
        , path_(path)
        , loader_(std::move(loader))
    {}

    Texture::~Texture() = default;

    void Texture::load() noexcept{
        assert(loader_ != nullptr);
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->full_path(path_.data());
        std::optional<Data> data = {};

        try{
            data = std::move(loader_->load(path));
            if (!data.has_value()){
                status_ = Status::eError;
                err_code_ = loader_->err_code();
                if (err_code_  == ErrorCode::eCannotOpenFile){
                    logger()->error("Can't open file: " + path);
                }
                else if (err_code_ == ErrorCode::eBadFormat){
                    logger()->error("Unsupported texture format: " + path);
                }
                return;
            }
        }catch(const std::exception& e){
            status_ = Status::eError;
            err_code_ = ErrorCode::eLoadingError;
            logger()->error("Can't load texture: " + path + ", error: " + std::string(e.what()));
        }

        try{
            const auto* renderer = resource_manager_->engine()->renderer();
            const auto& vd = renderer->vk_device();
            const auto& cmd_group = vd->queue_group(to<std::size_t>(rendering::Renderer::CommandGroup::eGraphicsAndPresent));

            // Получить формат в зависимости от кол-ва байт на пиксель
            const vk::Format desired_format = get_vk_format(data->channels, data->channel_depth);
            assert(desired_format != vk::Format::eUndefined);

            // Проверить доступность формата
            const auto fp = vd->physical_device().getFormatProperties(desired_format);
            if (!(fp.linearTilingFeatures & vk::FormatFeatureFlagBits::eTransferSrc))
            {
                status_ = Status::eError;
                err_code_ = ErrorCode::eVulkanError;
                logger()->error("Format not supported for linear tiling and transfer src: " + path);
                return;
            }
            if (!(fp.optimalTilingFeatures &
                (vk::FormatFeatureFlagBits::eSampledImage |
                vk::FormatFeatureFlagBits::eTransferDst |
                vk::FormatFeatureFlagBits::eTransferSrc)))
            {
                status_ = Status::eError;
                err_code_ = ErrorCode::eVulkanError;
                logger()->error("Format not supported for optimal tiling, transfer dst/src, or sampled image: " + path);
                return;
            }

            // Создать временное изображение
            const auto staging_image = std::make_unique<vk::utils::Image>(vd
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
            image_ = std::make_unique<vk::utils::Image>(vd
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
            const auto isl = vd->logical_device().getImageSubresourceLayout(
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
            image_->generate_mipmaps(cmd_group
                , vk::Extent3D{data->width, data->height, 1}
                , vk::ImageAspectFlagBits::eColor
                , 1);
        }
        catch([[maybe_unused]] const std::exception& e){
            status_ = Status::eError;
            err_code_ = ErrorCode::eVulkanError;
            logger()->error("Can't create vulkan texture: " + path);
            return;
        }

        status_ = Status::eLoaded;
    }

    rendering::Handles::Texture Texture::render_handles() const{
        return {
            image_->image(),
            image_->image_view()
        };
    }

    vk::Format Texture::get_vk_format(const uint32_t channels, const uint32_t channel_depth){
        // Байт (8 бит) на канал
        if (channel_depth == 1) {
            switch (channels) {
            case 1: return vk::Format::eR8Unorm;
            case 2: return vk::Format::eR8G8Unorm;
            case 3: return vk::Format::eR8G8B8Unorm;
            case 4: return vk::Format::eR8G8B8A8Unorm;
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
