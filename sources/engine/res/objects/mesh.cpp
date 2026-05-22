#include "pch.h"
#include <nasral/res/objects/mesh.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Mesh::Mesh(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader)
        : Resource(manager, id, Type::eMesh)
        , loader_(std::move(loader))
        , vertex_count_(0)
        , index_count_(0)
    {
    }

    Mesh::~Mesh()
    {
        vertex_buffer_.reset();
        index_buffer_.reset();
        RES_LOG_DESTRUCTION();
    }

    void Mesh::load() noexcept
    {
        assert(loader_ != nullptr && "Loader is null");
        if (status() == Status::eLoaded){
            return;
        }

        const auto full_path = subsystem()->path(id(), true);

        try
        {
            const auto data = loader_->load(full_path);

            if (!data.has_value()){
                throw std::runtime_error("Failed to load mesh file: " + full_path);
            }

            if (data->materials.size() > gfx::kMaxMaterialsPerMesh){
                throw std::runtime_error("Too many materials in mesh: " + full_path);
            }

            // Кол-во вершин и индексов
            vertex_count_ = static_cast<uint32_t>(data->vertices.size());
            index_count_ = static_cast<uint32_t>(data->indices.size());
            surfaces_ = data->materials;

            // Рендерер (получить)
            const auto* renderer = subsystem()
                ->engine()
                ->gfx();

            // Группа команд (для команд копирования из staging в целевое)
            auto& cmd_group = renderer
                ->vk_device()
                .queue_group(static_cast<size_t>(gfx::Renderer::CmdGroupType::eTransfer));

            // Вершины
            {
                // Создать временный буфер вершин (память ОЗУ)
                vk::utils::Buffer staging_buffer(
                    &renderer->vk_device(),
                    sizeof(gfx::Vertex) * vertex_count_,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

                // Итоговый буфер вершин
                vertex_buffer_ = std::make_unique<vk::utils::Buffer>(
                    &renderer->vk_device(),
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
                    &renderer->vk_device(),
                    sizeof(uint32_t) * index_count_,
                    vk::BufferUsageFlagBits::eTransferSrc,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

                // Итоговый буфер индексов
                index_buffer_ = std::make_unique<vk::utils::Buffer>(
                    &renderer->vk_device(),
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
        catch (const std::exception& e){
            set_status(Status::eError);
            set_error(error() == Error::eNone ? loader_->error() : error());
            RES_LOG_ERROR(error(), e.what());
        }

        set_status(Status::eLoaded);
        set_error(Error::eNone);
        RES_LOG_LOADED();
    }

    gfx::handles::Mesh Mesh::render_handles() const
    {
        gfx::handles::Mesh mesh_handle{};
        mesh_handle.vertex_buffer = vk_vertex_buffer();
        mesh_handle.index_buffer = vk_index_buffer();

        const auto copy_size = std::min<size_t>(surfaces_.size(), gfx::kMaxMaterialsPerMesh);

        if (copy_size > 0) {
            std::memcpy(
                mesh_handle.surfaces.data(),
                surfaces_.data(),
                copy_size * sizeof(gfx::handles::Mesh::Surface));
        }

        mesh_handle.surfaces_count = static_cast<uint32_t>(copy_size);
        return mesh_handle;
    }
}
