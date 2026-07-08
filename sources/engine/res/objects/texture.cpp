#include "pch.h"
#include <nasral/gfx/utils.h>
#include <nasral/res/objects/texture.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Texture::Texture(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader)
        : Resource(manager, id, Type::eTexture)
        , loader_(std::move(loader))
    {}

    Texture::~Texture(){
        image_.reset();
        RES_LOG_DESTRUCTION();
    }

    void Texture::load() noexcept
    {
        assert(loader_ != nullptr && "Loader is null");
        if (status() == Status::eLoaded){
            return;
        }

        const auto full_path = subsystem()->path(id(), true);

        try{
            const auto data = loader_->load(full_path);
            if (!data.has_value()){
                throw std::runtime_error("Failed to load image file: " + full_path);
            }

            // Параметры загрузки текстуры
            const auto* lp = loader_->params<TextureLoadParams>();

            // Группа команд (для команд копирования из staging в целевое)
            auto& cmd_group = subsystem()
                ->engine()
                ->gfx()
                ->renderer()
                ->vk_device()
                .queue_group(static_cast<size_t>(gfx::Renderer::CmdGroupType::eGraphicsAndPresent));

            // Получить формат в зависимости от кол-ва байт на пиксель
            const auto desired_format = gfx::get_image_vk_format(
                data->channels,
                data->channel_depth,
                lp ? lp->srgb : false);

            assert(desired_format != vk::Format::eUndefined && "Failed to get valid image format");

            // Проверить доступность формата
            const auto fp = subsystem()
                ->engine()
                ->gfx()
                ->renderer()
                ->vk_device()
                .physical_device()
                .getFormatProperties(desired_format);

            if (!(fp.optimalTilingFeatures & vk::FormatFeatureFlagBits::eTransferSrc))
            {
                set_error(Error::eVulkanError);
                throw std::runtime_error("Format not supported for linear tiling and transfer src: " + full_path);
            }

            if (!(fp.optimalTilingFeatures &
                (vk::FormatFeatureFlagBits::eSampledImage |
                vk::FormatFeatureFlagBits::eTransferDst |
                vk::FormatFeatureFlagBits::eTransferSrc)))
            {
                set_error(Error::eVulkanError);
                throw std::runtime_error("Format not supported for optimal tiling and transfer dst/src: " + full_path);
            }

            // Создать временное изображение
            const auto staging_image = std::make_unique<vk::utils::Image>(
                &subsystem()->engine()->gfx()->renderer()->vk_device()
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
            image_ = std::make_unique<vk::utils::Image>(
                &subsystem()->engine()->gfx()->renderer()->vk_device()
                , vk::utils::Image::Type::e2D
                , desired_format
                , vk::Extent3D{data->width, data->height, 1}
                , vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled
                , vk::ImageTiling::eOptimal
                , vk::ImageAspectFlagBits::eColor
                , vk::MemoryPropertyFlagBits::eDeviceLocal
                , vk::ImageLayout::ePreinitialized
                , vk::SampleCountFlagBits::e1
                , lp && lp->generate_mipmaps ? 0 : 1  // Автоматически создать мип-уровни если нужно (или 1, если нет)
                , 1);                                 // Кол-вл слоев (1 слой - обычная текстура)

            // Копировать данные (загруженные пиксели) в промежуточное изображение
            auto* mem = static_cast<uint8_t*>(staging_image->map(vk::ImageAspectFlagBits::eColor));
            const auto isl = subsystem()
                ->engine()
                ->gfx()
                ->renderer()
                ->vk_device()
                .logical_device()
                .getImageSubresourceLayout(staging_image->image(),{vk::ImageAspectFlagBits::eColor, 0, 0});

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

            // Генерируем ли mip-уровни (если mip уровни генерируются, изображение не нужно подготавливать к показу не надо)
            const bool generate_mips = image_->mip_levels() > 1 && (lp ? lp->generate_mipmaps : false);

            // Копировать данные из временного изображения в целевое
            staging_image->copy_to(*image_
                , cmd_group
                , vk::Extent3D{data->width, data->height, 1}
                , vk::ImageAspectFlagBits::eColor
                , vk::ImageAspectFlagBits::eColor
                , 1
                , 1
                , !generate_mips);

            // Генерация мип-уровней
            if (generate_mips){
                image_->generate_mipmaps(cmd_group
                    , vk::Extent3D{data->width, data->height, 1}
                    , vk::ImageAspectFlagBits::eColor
                    , 1);
            }
        }
        catch (const std::exception& e){
            set_status(Status::eError);
            set_error(error() == Error::eNone ? loader_->error() : error());
            RES_LOG_ERROR(error(), e.what());
            return;
        }

        set_status(Status::eLoaded);
        set_error(Error::eNone);
        RES_LOG_LOADED();
    }
}
