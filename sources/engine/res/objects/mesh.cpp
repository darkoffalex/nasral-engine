#include "pch.h"
#include <nasral/res/objects/mesh.h>
#include <nasral/gfx/utils.h>
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
                ->gfx()
                ->renderer();

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
                    vk::BufferUsageFlagBits::eVertexBuffer |
                    vk::BufferUsageFlagBits::eTransferDst |
                    vk::BufferUsageFlagBits::eShaderDeviceAddress |
                    vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
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
                    vk::BufferUsageFlagBits::eIndexBuffer |
                    vk::BufferUsageFlagBits::eTransferDst |
                    vk::BufferUsageFlagBits::eShaderDeviceAddress |
                    vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);

                // Копировать данные во временный (staging) буфер
                auto* p = staging_buffer.map_unsafe();
                memcpy(p, data->indices.data(), sizeof(uint32_t) * index_count_);
                staging_buffer.unmap_unsafe();

                // Копировать из временного в основной
                staging_buffer.copy_to(*index_buffer_, cmd_group);
            }

            // Построение структуры ускорения нижнего уровня (BLAS) для трассировки лучей
            build_blas();
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
        mesh_handle.blas_device_address = blas()->device_address();

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

    void Mesh::build_blas()
    {
        const auto* renderer = engine()->gfx()->renderer();
        auto& device = renderer->vk_device();
        auto& logical_device = device.logical_device();

        // Описание геометрии (треугольники)
        // Формат вершин зависит от структуры gfx::Vertex (Position Vec3 = eR32G32B32Sfloat)
        vk::AccelerationStructureGeometryTrianglesDataKHR triangles_data{};
        triangles_data.setVertexFormat(vk::Format::eR32G32B32Sfloat)
            .setVertexData(vertex_buffer_->device_address())
            .setVertexStride(sizeof(gfx::Vertex))
            .setMaxVertex(vertex_count_)
            .setIndexType(vk::IndexType::eUint32)
            .setIndexData(index_buffer_->device_address());

        vk::AccelerationStructureGeometryKHR geometry{};
        geometry.setGeometryType(vk::GeometryTypeKHR::eTriangles)
                .setFlags(vk::GeometryFlagBitsKHR::eOpaque) // или eNoDuplicateAnyHitInvocation при необходимости
                .setGeometry({triangles_data});

        // Описание сборки (Build Geometry Info)
        vk::AccelerationStructureBuildGeometryInfoKHR build_info{};
        build_info.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
                  .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
                  .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
                  .setGeometries(geometry);

        // Запрос необходимых размеров (размер BLAS и scratch-буфера)
        const uint32_t primitive_count = index_count_ / 3;
        const auto size_info = logical_device.getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_info,
            primitive_count,
            renderer->vk_loader()
        );

        // Создание BLAS и выделение памяти под него
        blas_ = std::make_unique<vk::utils::AccelerationStructure>(
            &device,
            vk::AccelerationStructureTypeKHR::eBottomLevel,
            size_info.accelerationStructureSize,
            renderer->vk_loader()
        );

        // Запрашиваем свойства физического устройства, передавая нужную структуру в качестве шаблона
        auto properties2 = device.physical_device().getProperties2<
            vk::PhysicalDeviceProperties2,
            vk::PhysicalDeviceAccelerationStructurePropertiesKHR
        >();

        // Достаем структуру свойств ускоряющих структур (нужна для выравнивания scratch-буфера)
        const auto& as_properties = properties2.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

        // Создание временного Scratch-буфера для вычислений GPU
        // Важно: scratch buffer требует выравнивания minAccelerationStructureScratchOffsetAlignment
        const auto scratch_buffer = std::make_unique<vk::utils::Buffer>(
            &device,
            gfx::size_align(size_info.buildScratchSize, as_properties.minAccelerationStructureScratchOffsetAlignment),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        // Привязываем целевой BLAS и scratch-буфер к информации о сборке
        build_info.setDstAccelerationStructure(blas_->handle()).setScratchData(scratch_buffer->device_address());

        // Диапазон построения примитивов
        vk::AccelerationStructureBuildRangeInfoKHR range_info{};
        range_info.setPrimitiveCount(primitive_count)
                  .setPrimitiveOffset(0)
                  .setFirstVertex(0)
                  .setTransformOffset(0);

        // Группа команд
        auto& cmd_group = renderer
            ->vk_device()
            .queue_group(static_cast<size_t>(gfx::Renderer::CmdGroupType::eGraphicsAndPresent));

        // Получить очередь для команд нужного типа  и mutex для защиты
        const auto& queue = cmd_group.queues.back();
        const auto& pool = cmd_group.command_pools.back();

        // Командный буфер (выделяется из пула)
        vk::UniqueCommandBuffer cmd_buffer{};

        // Забор для ожидания выполнения буфера
        vk::UniqueFence fence = logical_device.createFenceUnique(vk::FenceCreateInfo());

        // Критическая секция (защищаем очередь от конкурентного доступа)
        // Выделить командный буфер, записать команды построения BLAS, отправить их на исполнение
        {
            // Защитить секцию
            std::lock_guard lock(cmd_group.queue_mutexes.back());

            // Выделить командный буфер из пула
            auto cmd_buffers = logical_device.allocateCommandBuffersUnique(
                vk::CommandBufferAllocateInfo()
                .setCommandBufferCount(1)
                .setCommandPool(pool.get())
                .setLevel(vk::CommandBufferLevel::ePrimary));
            cmd_buffer = std::move(cmd_buffers.front());

            // Начать запись команд
            cmd_buffer.get().begin(
                vk::CommandBufferBeginInfo()
                .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

            // Команды построения BLAS
            cmd_buffer->buildAccelerationStructuresKHR(build_info, &range_info, renderer->vk_loader());
            cmd_buffer->pipelineBarrier(
                vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
                vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
                {},
                vk::MemoryBarrier().
                    setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR).
                    setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR),
                nullptr,
                nullptr);

            // Завершить запись команд
            cmd_buffer.get().end();

            // Отправить команду в очередь и подождать выполнения
            queue.submit(vk::SubmitInfo().setCommandBuffers({cmd_buffer.get()}), fence.get());
        }

        // Подождать выполнения
        (void)logical_device.waitForFences(
            {fence.get()},
            true,
            std::numeric_limits<uint64_t>::max());

        // Критическая секция (сброс командного буфера, вернуть в пул)
        {
            auto& queue_mutex = cmd_group.queue_mutexes.back();
            std::lock_guard lock(queue_mutex);
            cmd_buffer.reset();
        }
    }
}
