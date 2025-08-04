#include "pch.h"
#include <nasral/engine.h>
#include <nasral/resources/mesh.h>

namespace nasral::resources
{
    Mesh::Mesh(const ResourceManager* manager, const std::string_view& path, std::unique_ptr<Loader<Data>> loader)
        : IResource(Type::eMesh, manager, manager->engine()->logger())
        , path_(path)
        , loader_(std::move(loader))
        , vertex_count_(0)
        , index_count_(0)
    {}

    Mesh::~Mesh() = default;

    void Mesh::load() noexcept{
        assert(loader_ != nullptr);
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->full_path(path_.data());

        try{
            const auto data = loader_->load(path);
            if (!data.has_value()){
                status_ = Status::eError;
                err_code_ = loader_->err_code();
                if (err_code_  == ErrorCode::eCannotOpenFile){
                    logger()->error("Can't open file: " + path);
                }else if (err_code_ == ErrorCode::eBadFormat){
                    logger()->error("Wrong mesh data: " + path);
                }
                return;
            }

            // Кол-во вершин и индексов
            vertex_count_ = data->vertices.size();
            index_count_ = data->indices.size();

            // Получить устройство и группу очередей для копирования/перемещения
            const auto* renderer = resource_manager_->engine()->renderer();
            const auto& vd = renderer->vk_device();
            const auto& transfer_group = vd->queue_group(to<std::size_t>(rendering::Renderer::CommandGroup::eTransfer));

            // Вершины
            {
                // Создать временный буфер вершин (память ОЗУ)
                vk::utils::Buffer staging_buffer(
                    vd,
                    sizeof(rendering::Vertex) * vertex_count_,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

                // Итоговый буфер вершин
                vertex_buffer_ = std::make_unique<vk::utils::Buffer>(
                    vd,
                    sizeof(rendering::Vertex) * vertex_count_,
                    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);

                // Копировать данные во временный (staging) буфер
                auto* p = staging_buffer.map_unsafe();
                memcpy(p, data->vertices.data(), sizeof(rendering::Vertex) * vertex_count_);
                staging_buffer.unmap_unsafe();

                // Копировать из временного в основной
                staging_buffer.copy_to(*vertex_buffer_, transfer_group);
            }

            // Индексы
            {
                // Создать временный буфер индексов (память ОЗУ)
                vk::utils::Buffer staging_buffer(
                    vd,
                    sizeof(uint32_t) * index_count_,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

                // Итоговый буфер индексов
                index_buffer_ = std::make_unique<vk::utils::Buffer>(
                    vd,
                    sizeof(uint32_t) * index_count_,
                    vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);

                // Копировать данные во временный (staging) буфер
                auto* p = staging_buffer.map_unsafe();
                memcpy(p, data->indices.data(), sizeof(uint32_t) * index_count_);
                staging_buffer.unmap_unsafe();

                // Копировать из временного в основной
                staging_buffer.copy_to(*index_buffer_, transfer_group);
            }
        }
        catch([[maybe_unused]] const std::exception& e){
            vertex_buffer_.reset();
            index_buffer_.reset();
            vertex_count_ = 0;
            index_count_ = 0;

            status_ = Status::eError;
            err_code_ = ErrorCode::eVulkanError;
            logger()->error("Can't create mesh resource: " + path);
            return;
        }

        status_ = Status::eLoaded;
    }

    rendering::Handles::Mesh Mesh::render_handles() const{
        return {
            vk_vertex_buffer(),
            vk_index_buffer(),
            static_cast<uint32_t>(index_count_),
        };
    }
}
