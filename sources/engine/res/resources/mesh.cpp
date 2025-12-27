#include "pch.h"
#include <nasral/res/resources/mesh.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Mesh::Mesh(Manager* manager, const ResourceId id, Loader<Data>::Ptr loader)
        : IResource(Type::eMesh, id, manager)
        , loader_(std::move(loader))
        , vertex_count_(0)
        , index_count_(0)
    {}

    Mesh::~Mesh(){
        index_buffer_.reset();
        vertex_buffer_.reset();
        RES_LOG_DESTRUCTION();
    }

    void Mesh::load() noexcept
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
                throw std::runtime_error("Failed to load mesh:" + path);
            }

            // Кол-во вершин и индексов
            vertex_count_ = data->vertices.size();
            index_count_ = data->indices.size();

            constexpr auto group_idx = static_cast<size_t>(gfx::Renderer::CommandGroup::eTransfer);
            const auto renderer = manager()->engine()->renderer();
            auto& cmd_group = renderer->vk_device().queue_group(group_idx);

            // Вершины
            {
                // Создать временный буфер вершин (память ОЗУ)
                vk::utils::Buffer staging_buffer(
                    renderer->vk_device_ptr(),
                    sizeof(gfx::Vertex) * vertex_count_,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

                // Итоговый буфер вершин
                vertex_buffer_ = std::make_unique<vk::utils::Buffer>(
                    renderer->vk_device_ptr(),
                    sizeof(gfx::Vertex) * vertex_count_,
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);

                // Копировать данные во временный (staging) буфер
                auto* p = staging_buffer.map_unsafe();
                memcpy(p, data->vertices.data(), sizeof(gfx::Vertex) * vertex_count_);
                staging_buffer.unmap_unsafe();

                // Копировать из временного в основной
                staging_buffer.copy_to(*vertex_buffer_, cmd_group);
            }

            // Индексы
            {
                // Создать временный буфер индексов (память ОЗУ)
                vk::utils::Buffer staging_buffer(
                    renderer->vk_device_ptr(),
                    sizeof(uint32_t) * index_count_,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

                // Итоговый буфер индексов
                index_buffer_ = std::make_unique<vk::utils::Buffer>(
                    renderer->vk_device_ptr(),
                    sizeof(uint32_t) * index_count_,
                    vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);

                // Копировать данные во временный (staging) буфер
                auto* p = staging_buffer.map_unsafe();
                memcpy(p, data->indices.data(), sizeof(uint32_t) * index_count_);
                staging_buffer.unmap_unsafe();

                // Копировать из временного в основной
                staging_buffer.copy_to(*index_buffer_, cmd_group);
            }
        }
        catch (const std::exception& e)
        {
            status_ = Status::eError;
            error_ = error_ == Error::eNone ? Error::eLoadingFailed : error_;
            RES_LOG_ERROR(error_, e.what());
            return;
        }

        status_ = Status::eLoaded;
        error_ = Error::eNone;
        RES_LOG_LOADED();
    }
}
