#include "pch.h"
#include <nasral/rendering/renderer.h>
#include <nasral/engine.h>

namespace nasral::rendering
{
    Renderer::Renderer(const Engine *engine, RenderingConfig config)
        : engine_(engine)
        , config_(std::move(config))
        , is_rendering_(false)
        , surface_refresh_required_(false)
        , current_frame_(0)
        , available_image_index_(0)
    {
        logger()->info("Initializing renderer...");

        try {
            init_vk_instance();
            logger()->info("Vulkan: Instance created.");

            init_vk_loader();
            logger()->info("Vulkan: Loader created.");

            init_vk_debug_callback();
            logger()->info("Vulkan: Debug callback created.");

            init_vk_surface();
            logger()->info("Vulkan: Surface created.");

            init_vk_device();
            const auto device_name = vk_device_->physical_device().getProperties().deviceName;
            const auto device_name_str = std::string(device_name.data(), strlen(device_name.data()));
            logger()->info("Vulkan: Device initialized (" + device_name_str + ").");

            init_vk_render_passes();
            logger()->info("Vulkan: Render passes created.");

            init_vk_swap_chain();
            logger()->info("Vulkan: Swap chain created.");

            init_vk_framebuffers();
            const auto extent = vk_framebuffers_[0]->extent();
            const std::string extent_str = std::to_string(extent.width) + "x" + std::to_string(extent.height);
            logger()->info("Vulkan: Frame buffers created (" + extent_str + ").");

            init_vk_uniform_layouts();
            logger()->info("Vulkan: Uniform layouts created.");

            init_vk_texture_samplers();
            logger()->info("Vulkan: Texture samplers created.");

            init_vk_uniforms();
            logger()->info("Vulkan: Uniform buffers allocated.");

            init_vk_command_buffers();
            logger()->info("Vulkan: Command buffers created.");

            init_vk_sync_objects();
            logger()->info("Vulkan: Sync primitives created.");

            init_index_pools();
            logger()->info("Index pools initialized.");

            is_rendering_ = true;
        }
        catch (const std::exception& e) {
            throw RenderingError(e.what());
        }
    }

    Renderer::~Renderer()
    {
        is_rendering_ = false;
        cmd_wait_for_frame();
    }

    void Renderer::cmd_begin_frame(){
        // Если требуется обновить поверхность и связанные с ней кадровые буферы
        if (surface_refresh_required_.exchange(false, std::memory_order_acquire)){
            refresh_vk_surface();
        }

        // Если рендеринг отключен
        if (!is_rendering_) return;

        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);

        // Размеры области рендеринга
        const auto& extent = vk_framebuffers_[frame_index]->extent();
        const auto& width = extent.width;
        const auto& height = extent.height;

        // Описываем очистку вложений кадрового буфера (цвет, глубина/трафарет)
        std::array<vk::ClearValue, 2> clear_values{};
        clear_values[0].color = vk::ClearColorValue(config_.clear_color);
        clear_values[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

        // Ожидаем завершения кадра с текущим индексом (на случай если он еще не готов)
        // Функция блокирует поток при ожидании барьера
        (void)vk_device_->logical_device().waitForFences(
            1u,
            &vk_frame_fence_[frame_index].get(),
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        // Сброс барьера кадра
        (void)vk_device_->logical_device().resetFences(
            1u,
            &vk_frame_fence_[frame_index].get());

        // Получить доступное изображение swap-chain
        // Функция блокирует поток до получения доступного изображения.
        const auto result = vk_device_->logical_device().acquireNextImageKHR(
            vk_swap_chain_.get(),
            std::numeric_limits<uint64_t>::max(),
            vk_render_available_semaphore_[frame_index].get(),
            VK_NULL_HANDLE,
            &available_image_index_);

        // Если изображение было получено
        if (result == vk::Result::eSuccess){
            // Получить буфер кадра и команд
            auto& cmd_buffer = vk_command_buffers_[frame_index];
            auto& frame_buffer = vk_framebuffers_[available_image_index_]->vk_framebuffer();

            // Начать работу с буфером команд
            cmd_buffer->reset();
            cmd_buffer->begin(vk::CommandBufferBeginInfo());

            // Убедиться, что все необходимые storage buffer'ы доступны
            /*
            vk::BufferMemoryBarrier barrier{};
            barrier.setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(vk_ubo_objects_transforms_->vk_buffer())
                .setSize(vk_ubo_objects_transforms_->size())
                .setOffset(0);

            cmd_buffer->pipelineBarrier(
                vk::PipelineStageFlagBits::eHost,
                vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader,
                {},
                {},
                {barrier},
                {});
            */

            // Начать проход рендеринга, используя полученный ранее кадровый буфер
            cmd_buffer->beginRenderPass(
                vk::RenderPassBeginInfo()
                .setRenderPass(vk_render_pass_.get())
                .setFramebuffer(frame_buffer)
                .setRenderArea(vk::Rect2D(
                    vk::Offset2D(0, 0),
                    vk::Extent2D(width, height)))
                .setClearValues(clear_values),
                vk::SubpassContents::eInline);
        }
        // Возможно требуется пересоздание swap-chain
        else if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR){
            request_surface_refresh();
        }
    }

    void Renderer::cmd_end_frame(){
        // Если рендеринг отключен
        if (!is_rendering_) return;

        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame_index];

        // Завершение прохода (неявное преобразование кадра в VK_IMAGE_LAYOUT_PRESENT_SRC_KHR для представления)
        cmd_buffer->endRenderPass();

        // Завершения командного буфера
        cmd_buffer->end();

        // Семафоры, ожидаемые для исполнения команд рендеринга
        std::array<vk::Semaphore, 1> wait_semaphores{
            vk_render_available_semaphore_[frame_index].get()
        };

        // Семафоры, сигнализирующие готовность к показу
        std::array<vk::Semaphore, 1> signal_semaphores{
            vk_render_finished_semaphore_[frame_index].get()
        };

        // Стадии, на которых конвейер будет ждать wait_semaphores
        std::array<vk::PipelineStageFlags, 1> wait_stages{
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        // Отправить командные буферы на исполнение
        const auto& group = vk_device_->queue_group(to<size_t>(CommandGroup::eGraphicsAndPresent));
        auto& queue = group.queues[0];

        // Подача команд рендеринга в очередь
        queue.submit(vk::SubmitInfo()
            .setCommandBuffers(cmd_buffer.get())
            .setWaitSemaphores(wait_semaphores)
            .setWaitDstStageMask(wait_stages)
            .setSignalSemaphores(signal_semaphores),
            vk_frame_fence_[frame_index].get());

        // В случае ошибки показа - вероятно требуется пересоздание swap-chain
        try
        {
            // Подача команд показа в очередь
            const auto result = queue.presentKHR(vk::PresentInfoKHR()
                .setSwapchains(vk_swap_chain_.get())
                .setWaitSemaphores(signal_semaphores)
                .setImageIndices(available_image_index_));

            if (result == vk::Result::eErrorOutOfDateKHR){
                request_surface_refresh();
                return;
            }
        }
        catch(const ::vk::OutOfDateKHRError&){
            request_surface_refresh();
            return;
        }

        // Инкремент счетчика кадров
        current_frame_++;
    }

    void Renderer::cmd_bind_material(const Handles::Material& handles, const uint32_t mat_index){
        // Если рендеринг отключен
        if (!is_rendering_) return;

        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame_index];

        // Размеры области рендеринга
        const auto& extent = vk_framebuffers_[frame_index]->extent();
        const auto& width = extent.width;
        const auto& height = extent.height;

        // Область вида (динамическое состояние)
        auto viewport = vk::Viewport()
        .setX(0.0f)
        .setWidth(static_cast<float>(width))
        .setMinDepth(0.0f)
        .setMaxDepth(1.0f);

        if (config_.use_opengl_style){
            viewport.setY(static_cast<float>(height));
            viewport.setHeight(-static_cast<float>(height));
        }else{
            viewport.setY(0.0f);
            viewport.setHeight(static_cast<float>(height));
        }

        // Ножницы (динамическое состояние)
        auto scissor = vk::Rect2D()
        .setOffset(vk::Offset2D(0, 0))
        .setExtent(extent);

        // Получить макет конвейера
        const auto& ul = vk_uniform_layouts_[to<size_t>(UniformLayoutType::eBasicRasterization)];
        static const auto& pl = ul->vk_pipeline_layout();

        // Передать индекс материала через push constant
        cmd_buffer->pushConstants(
            pl,
            vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment,
            0,
            sizeof(uint32_t),
            &mat_index);

        // Запись команд. Привязать конвейер и динамические состояния
        cmd_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, handles.pipeline);
        cmd_buffer->setViewport(0, {viewport});
        cmd_buffer->setScissor(0, {scissor});
    }

    void Renderer::cmd_bind_frame_descriptors(){
        // Если рендеринг отключен
        if (!is_rendering_) return;
        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);
        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame_index];
        // Получить макет конвейера
        const auto& ul = vk_uniform_layouts_[to<size_t>(UniformLayoutType::eBasicRasterization)];
        static const auto& pl = ul->vk_pipeline_layout();
        // Привязать все необходимые дескрипторы
        cmd_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            pl,
            0,
            {
                vk_dset_view_.get(),
                vk_dset_objects_uniforms_.get(),
                vk_dset_material_uniforms_.get(),
                vk_dset_material_textures_.get(),
                vk_dset_light_sources_.get()
            },
            {});
    }

    void Renderer::cmd_draw_mesh(const Handles::Mesh& handles, const uint32_t obj_index){
        // Если рендеринг отключен
        if (!is_rendering_) return;

        // Текущий индекс кадра
        const auto frame_index = current_frame_ % static_cast<size_t>(config_.max_frames_in_flight);

        // Получить буфер команд
        auto& cmd_buffer = vk_command_buffers_[frame_index];

        // Получить макет конвейера
        const auto& ul = vk_uniform_layouts_[to<size_t>(UniformLayoutType::eBasicRasterization)];
        static const auto& pl = ul->vk_pipeline_layout();

        // Передать индекс объекта через push constant
        cmd_buffer->pushConstants(
            pl,
            vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment,
            sizeof(uint32_t),
            sizeof(uint32_t),
            &obj_index);

        // Запись команд. Привязать геометрию и нарисовать её
        cmd_buffer->bindVertexBuffers(0, {handles.vertex_buffer}, {0});
        cmd_buffer->bindIndexBuffer(handles.index_buffer, 0, vk::IndexType::eUint32);
        cmd_buffer->drawIndexed(handles.index_count, 1, 0, 0, 0);
    }

    void Renderer::cmd_wait_for_frame() const{
        vk_device_->logical_device().waitIdle();
    }

    void Renderer::request_surface_refresh(){
        surface_refresh_required_.store(true, std::memory_order_release);
    }

    void Renderer::update_cam_ubo(const uint32_t index, const CameraUniforms& uniforms) const{
        assert(vk_ubo_view_->is_mapped());
        vk_ubo_view_->update_mapped(
            ubo_offset<CameraUniforms>(index),
            aligned_ubo_size<CameraUniforms>(),
            &uniforms);
    }

    void Renderer::update_obj_ubo(const uint32_t index, const ObjectTransformUniforms& uniforms) const{
        assert(vk_ubo_objects_transforms_->is_mapped());
        vk_ubo_objects_transforms_->update_mapped(
            sbo_offset<ObjectTransformUniforms>(index),
            aligned_sbo_size<ObjectTransformUniforms>(),
            &uniforms);
    }

    void Renderer::update_material_ubo(const uint32_t index, const MaterialPhongUniforms& uniforms) const{
        assert(vk_ubo_materials_phong_->is_mapped());
        vk_ubo_materials_phong_->update_mapped(
            sbo_offset<MaterialPhongUniforms>(index),
            aligned_sbo_size<MaterialPhongUniforms>(),
            &uniforms);
    }

    void Renderer::update_material_ubo(const uint32_t index, const MaterialPbrUniforms& uniforms) const{
        assert(vk_ubo_materials_pbr_->is_mapped());
        vk_ubo_materials_pbr_->update_mapped(
            sbo_offset<MaterialPbrUniforms>(index),
            aligned_sbo_size<MaterialPbrUniforms>(),
            &uniforms);
    }

    void Renderer::update_material_tex(const uint32_t index, const TextureBindingInfo& info) const{
        assert(index < MAX_MATERIALS);
        assert(info.texture);
        assert(vk_dset_material_textures_);

        const auto& sampler = vk_texture_samplers_[to<size_t>(info.sampler_type)];
        vk::DescriptorImageInfo image_info{};
        image_info.setSampler(sampler.get())
                  .setImageView(info.texture.image_view)
                  .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::WriteDescriptorSet write{};
        write.setDstSet(vk_dset_material_textures_.get())
             .setDstBinding(to<uint32_t>(info.type))
             .setDstArrayElement(index) // Индекс объекта в массиве дескрипторов
             .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
             .setDescriptorCount(1)
             .setImageInfo(image_info);

        vk_device_->logical_device().updateDescriptorSets({write}, {});
    }

    void Renderer::update_light_ubo(const uint32_t index, const LightUniforms &uniforms) const {
        assert(vk_ubo_light_sources_->is_mapped());
        vk_ubo_light_sources_->update_mapped(
            sbo_offset<LightUniforms>(index),
            aligned_sbo_size<LightUniforms>(),
            &uniforms);
    }

    uint32_t Renderer::obj_id_acquire_unsafe(){
        if (object_ids_.empty()){
            throw RenderingError("No more object IDs available");
        }

        const uint32_t id = object_ids_.back();
        object_ids_.pop_back();
        return id;
    }

    uint32_t Renderer::obj_id_acquire(){
        std::lock_guard lock(obj_ids_mutex_);
        return obj_id_acquire_unsafe();
    }

    void Renderer::obj_id_release_unsafe(const uint32_t id){
        assert(id < MAX_OBJECTS);
        object_ids_.push_back(id);
    }

    void Renderer::obj_id_release(const uint32_t id){
        std::lock_guard lock(obj_ids_mutex_);
        obj_id_release_unsafe(id);
    }

    void Renderer::obj_ids_reset_unsafe(){
        object_ids_.clear();
        for (uint32_t i = MAX_OBJECTS; i > 0; --i){
            object_ids_.push_back(i - 1);
        }
    }

    void Renderer::obj_ids_reset(){
        std::lock_guard lock(obj_ids_mutex_);
        obj_ids_reset_unsafe();
    }

    uint32_t Renderer::material_acquire_unsafe(
        const MaterialType type,
        const std::string& path,
        const std::vector<std::string>& tex_paths)
    {
        if (material_ids_.empty()){
            throw RenderingError("No more material IDs available");
        }

        const uint32_t id = material_ids_.back();
        material_ids_.pop_back();

        assert(id < MAX_MATERIALS);
        if (to<size_t>(id) >= materials_.size()){
            materials_.emplace_back(std::nullopt);
        }

        assert(materials_[id] == std::nullopt);
        materials_[id] = std::optional<MaterialInstance>({
            engine()->resource_manager(),
            type,
            path,
            tex_paths
        });

        return id;
    }

    uint32_t Renderer::material_acquire(
        const MaterialType type,
        const std::string& path,
        const std::vector<std::string>& tex_paths)
    {
        std::lock_guard lock(materials_mutex_);
        return material_acquire_unsafe(type, path, tex_paths);
    }

    MaterialInstance& Renderer::material_instance_unsafe(const uint32_t id){
        assert(id < MAX_MATERIALS);
        if (materials_[id] == std::nullopt){
            throw RenderingError("Material ID is invalid");
        }
        return *materials_[id];
    }

    MaterialInstance& Renderer::material_instance(const uint32_t id){
        std::lock_guard lock(materials_mutex_);
        return material_instance_unsafe(id);
    }

    void Renderer::material_release_unsafe(const uint32_t id){
        assert(id < MAX_MATERIALS);
        if (materials_[id] == std::nullopt){
            throw RenderingError("Material ID is invalid");
        }

        materials_[id] = std::nullopt;
        material_ids_.push_back(id);
    }

    void Renderer::material_release(const uint32_t id){
        std::lock_guard lock(materials_mutex_);
        material_release_unsafe(id);
    }

    void Renderer::materials_reset_unsafe(){
        materials_.clear();
        material_ids_.clear();

        for (uint32_t i = MAX_MATERIALS; i > 0; --i){
            material_ids_.push_back(i - 1);
        }
    }

    void Renderer::materials_reset(){
        std::lock_guard lock(materials_mutex_);
        materials_reset_unsafe();
    }

    void Renderer::materials_update_unsafe(){
        for (size_t i = 0; i < materials_.size(); ++i){
            if (materials_[i].has_value()){
                const auto index = to<uint32_t>(i);
                auto& m = materials_[i].value();

                // Параметры материалов
                if (m.check_changes(MaterialInstance::eSettingsChanged, false, true)){
                    if (m.settings().has_value()){
                        auto& settings = m.settings().value();
                        std::visit([&](const auto& s){
                            using T = std::decay_t<decltype(s)>;
                            if constexpr (
                                std::is_same_v<T, MaterialPhongUniforms> ||
                                std::is_same_v<T, MaterialPbrUniforms>)
                            {
                                update_material_ubo(index, s);
                            }
                        }, settings);
                    }
                }

                // Текстуры материалов
                if (m.check_changes(MaterialInstance::eTextureChanged, false, true)){
                    for (uint32_t tt = 0; tt < to<uint32_t>(TextureType::TOTAL); ++tt){
                        if (const Handles::Texture& th = m.tex_render_handles(to<TextureType>(tt))){
                            TextureBindingInfo info{};
                            info.texture = th;
                            info.type = to<TextureType>(tt);
                            info.sampler_type = m.tex_sampler(info.type);
                            update_material_tex(index, info);
                        }
                    }
                }
            }
        }
    }

    uint32_t Renderer::light_id_acquire_unsafe() {
        if (light_ids_.empty()) {
            throw RenderingError("No more light IDs available");
        }

        const uint32_t id = light_ids_.back();
        light_ids_.pop_back();
        return id;
    }

    uint32_t Renderer::light_id_acquire() {
        std::lock_guard lock(light_ids_mutex_);
        return light_id_acquire_unsafe();
    }

    void Renderer::light_id_release_unsafe(const uint32_t id) {
        assert(id < MAX_LIGHTS);
        light_ids_.push_back(id);
    }

    void Renderer::light_id_release(const uint32_t id) {
        std::lock_guard lock(obj_ids_mutex_);
        light_id_release_unsafe(id);
    }

    void Renderer::light_ids_reset_unsafe() {
        light_ids_.clear();
        for (uint32_t i = MAX_LIGHTS; i > 0; --i) {
            light_ids_.push_back(i - 1);
        }
    }

    void Renderer::light_ids_reset() {
        std::lock_guard lock(obj_ids_mutex_);
        light_ids_reset_unsafe();
    }

    void Renderer::light_ids_activate_unsafe(const std::vector<uint32_t> &ids) {
        assert(vk_ubo_light_indices_->is_mapped());
        assert(std::all_of(ids.begin(), ids.end(), [](const uint32_t id) { return id < MAX_LIGHTS; }));
        assert(ids.size() <= MAX_LIGHTS);

        auto& active = active_light_ids_;
        for (const auto id : ids) {
            if (std::find(active.begin(), active.end(), id) == active.end()) {
                active.push_back(id);
            }
        }

        auto* pids = static_cast<LightIndices*>(vk_ubo_light_indices_->mapped_ptr());
        pids->count = static_cast<uint32_t>(active.size());
        std::fill_n(pids->indices, MAX_LIGHTS, 0);
        std::memcpy(pids->indices, active.data(), active.size() * sizeof(uint32_t));
    }

    void Renderer::light_ids_activate(const std::vector<uint32_t> &ids) {
        std::lock_guard lock(light_ids_mutex_);
        light_ids_activate_unsafe(ids);
    }

    void Renderer::light_ids_deactivate_unsafe(const std::vector<uint32_t> &ids) {
        if (!vk_ubo_light_indices_->is_mapped()) return;
        assert(std::all_of(ids.begin(), ids.end(), [](const uint32_t id) { return id < MAX_LIGHTS; }));
        assert(ids.size() <= MAX_LIGHTS);

        auto& active = active_light_ids_;
        for (const auto id : ids) {
            if (auto it = std::find(active.begin(), active.end(), id); it != active.end()) {
                active.erase(it);
            }
        }

        auto* pids = static_cast<LightIndices*>(vk_ubo_light_indices_->mapped_ptr());
        pids->count = static_cast<uint32_t>(active.size());
        std::fill_n(pids->indices, MAX_LIGHTS, 0);
        std::memcpy(pids->indices, active.data(), active.size() * sizeof(uint32_t));
    }

    void Renderer::light_ids_deactivate(const std::vector<uint32_t> &ids) {
        std::lock_guard lock(light_ids_mutex_);
        light_ids_deactivate_unsafe(ids);
    }

    VkBool32 Renderer::vk_debug_report_callback(
        [[maybe_unused]] vk::Flags<vk::DebugReportFlagBitsEXT> flags,
        [[maybe_unused]] vk::DebugReportObjectTypeEXT object_type,
        [[maybe_unused]] uint64_t obj,
        [[maybe_unused]] size_t location,
        [[maybe_unused]] int32_t code,
        const char* layer_prefix,
        const char* msg,
        void* user_data)
    {
        const auto* logger = static_cast<logging::Logger*>(user_data);
        const std::string msg_str = "Vulkan validation: " + std::string(layer_prefix) + " | " + std::string(msg) + "\n";
        logger->warning(msg_str);
        return VK_FALSE;
    }

    const logging::Logger* Renderer::logger() const{
        return engine()->logger();
    }

    void Renderer::init_vk_instance()
    {
        // Требуемые расширения и слои
        std::vector<const char*> req_extensions = config_.surface_provider->surface_extensions();
        std::vector<const char*> req_layers = {};

        // Если нужна валидация
        if (config_.use_validation_layers){
            req_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            req_layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        // Информация о приложении
        const auto app_info = vk::ApplicationInfo()
            .setPApplicationName(config_.app_name.c_str())
            .setPEngineName(config_.engine_name.c_str())
            .setApiVersion(VK_API_VERSION_1_4)
            .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
            .setEngineVersion(VK_MAKE_VERSION(1, 0, 0));

        // Создать сущность Vulkan
        vk_instance_ = vk::createInstanceUnique(vk::InstanceCreateInfo()
            .setPEnabledExtensionNames(req_extensions)
            .setPEnabledLayerNames(req_layers)
            .setPApplicationInfo(&app_info));
    }

    void Renderer::init_vk_loader(){
        assert(vk_instance_);
        vk_dispatch_loader_ = vk::detail::DispatchLoaderDynamic(vk_instance_.get(), config_.pfn_vk_get_proc_addr);
        vk_dispatch_loader_.init(vk_instance_.get());
    }

    void Renderer::init_vk_debug_callback(){
        if (!config_.use_validation_layers) return;
        assert(vk_instance_);

        // Используем статический метод vk_debug_report_callback для логирования предупреждений и ошибок от слоев.
        // Передаем logger как пользовательский указатель
        vk_debug_callback_ = vk_instance_->createDebugReportCallbackEXTUnique(
            vk::DebugReportCallbackCreateInfoEXT()
                .setFlags(vk::DebugReportFlagBitsEXT::eError|vk::DebugReportFlagBitsEXT::eWarning)
                .setPfnCallback(reinterpret_cast<vk::PFN_DebugReportCallbackEXT>(vk_debug_report_callback))
                .setPUserData(const_cast<logging::Logger*>(logger())),
            nullptr,
            vk_dispatch_loader_);
    }

    void Renderer::init_vk_surface(){
        assert(vk_instance_);
        // Поверхность создается вне системы рендеринга (на стороне приложения, например, при помощи GLFW)
        const vk::SurfaceKHR surface = config_.surface_provider->create_surface(*vk_instance_);

        // Поскольку был создан "голый" handler, нужно обернуть его в unique pointer
        // Также нужно предоставить функтор удаления (второй аргумент, используем стандартный)
        vk_surface_ = vk::UniqueSurfaceKHR(
            surface,
            ::vk::detail::ObjectDestroy<::vk::Instance, ::vk::detail::DispatchLoaderStatic>(vk_instance_.get()));
    }

    void Renderer::init_vk_device(){
        assert(vk_instance_);
        assert(vk_surface_);

        // Требуемые расширения (поддержка своп-чейна и выделенных аллокаций памяти)
        const std::vector req_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
        };

        // Требования к очередям
        // Первая группа содержит 2 графические очереди (рендеринг и операции загрузки текстур)
        // Вторая группа содержит 1 очередь команд переноса данных (загрузка ресурсов)
        std::vector<vk::utils::Device::QueueGroupRequest> req_queues(to<size_t>(CommandGroup::TOTAL));
        req_queues[to<size_t>(CommandGroup::eGraphicsAndPresent)] = vk::utils::Device::QueueGroupRequest::graphics(2, true),
        req_queues[to<size_t>(CommandGroup::eTransfer)] = vk::utils::Device::QueueGroupRequest::transfer(1),

        // Создать устройство
        vk_device_ = std::make_unique<vk::utils::Device>(
            vk_instance_,
            vk_surface_,
            req_queues,
            req_extensions);
    }

    void Renderer::init_vk_render_passes(){
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        if (!vk_device_->supports_color(config_.color_format, vk_surface_)){
            throw std::runtime_error("Color format is not supported by the device");
        }

        if (!vk_device_->supports_depth(config_.depth_stencil_format)){
            throw std::runtime_error("Depth stencil format is not supported by the device");
        }

        /**
         * Общая теория:
         *
         * В отличии от OpenGL, где проход редеринга абстракция чисто програмная, в Vulkan проход это отдельный объект.
         * Он описывает как ведет себя память вложений (цвет, глубина) на разных этапах конвейера рендеринга. У каждого
         * прохода есть свои под-проходы, они позволяют более оптимально работать с вложениями (например, получать к ним
         * доступ в шейдере до его записи в кадровый буер) за счет быстрой памяти устройство (своего рода кэш). Но стоит
         * также учитывать ограничения: при таких эффектах как размытие некоторые из тайлов могут быть не готовы, и их
         * данные будут недоступны. Под-проходы идеально подходят для реализации таких концепций как G-буфер, отложенный
         * рендеринг и прочие аналогичные техники. Каждому под-проходу может соответствовать свой отдельный конвейер
         * с шейдерами.
         */

        // Описания вложений.
        // Предполагается использование двух вложений - цвета и глубины/трафарета.
        // Вложение - изображение, в которое производится запись на стороне shader'а.
        // Вложение также может быть прочитано в другом под-проходе (например, для легкой пост-обработки)
        std::vector<::vk::AttachmentDescription> attachment_descriptions{};

        // Цвет
        attachment_descriptions.push_back(
            vk::AttachmentDescription()
            .setFormat(config_.color_format)
            .setSamples(vk::SampleCountFlagBits::e1)                        // Без multisampling (1 семпл)
            .setLoadOp(vk::AttachmentLoadOp::eClear)                        // Очистка вложение в начале под-прохода
            .setStoreOp(vk::AttachmentStoreOp::eStore)                      // Хранить для показа (один под-проход)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)              // Трафарет не используем (цветовое вложение)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)            // Трафарет не используем (цветовое вложение)
            .setInitialLayout(vk::ImageLayout::eUndefined)                  // Изначального макета памяти еще нет
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR)                // В конце - отправка на экран (один под-проход)
        );

        // Глубина/трафарет
        attachment_descriptions.push_back(
            vk::AttachmentDescription()
            .setFormat(config_.depth_stencil_format)
            .setSamples(vk::SampleCountFlagBits::e1)                        // Без multisampling (1 семпл)
            .setLoadOp(vk::AttachmentLoadOp::eClear)                        // Очистка вложение в начале под-прохода
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)                   // Хранить для показа не нужно (не показываем)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)              // Трафарет не используем (только глубина)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)            // Трафарет не используем (только глубина)
            .setInitialLayout(vk::ImageLayout::eUndefined)                  // Изначального макета памяти еще нет
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)); // В конце - использование в качестве вложения глубины/трафарета

        // Ссылки на вложения.
        // Указываем, какие вложения из ранее описанных (и в качестве чего) будет использовать под-проход.
        // На данный момент: 1 проход с 1 под-проходом (с последующим выводом изображения в кадровый буфер).
        std::vector<vk::AttachmentReference> color_attachment_refs{};
        std::vector<vk::AttachmentReference> depth_attachment_refs{};

        color_attachment_refs.push_back(
            vk::AttachmentReference()
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal));

        depth_attachment_refs.push_back(
            vk::AttachmentReference()
            .setAttachment(1)
            .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal));

        // Под-проходы (1 под проход на текущий момент)
        std::vector<vk::SubpassDescription> subpass_descriptions{};
        subpass_descriptions.push_back(
            vk::SubpassDescription().
            setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachments(color_attachment_refs)
            .setPDepthStencilAttachment(&depth_attachment_refs[0]));

        // Зависимости под-проходов.
        // Указываем, на каких стадиях конвейера какой будет доступ к вложениям под-проходов
        std::vector<vk::SubpassDependency> subpass_dependencies{};

        // Переход из внешнего (неявного) в основной (первый/нулевой)
        subpass_dependencies.push_back(
            vk::SubpassDependency()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)                                   // Исходный под-проход (внешний)
            .setDstSubpass(0)                                                     // Целевой (первый)
            .setSrcStageMask(vk::PipelineStageFlagBits::eTopOfPipe)               // Этап ожидания операций
            .setSrcAccessMask(vk::AccessFlagBits::eNone)                          // Нет операций для ожидания (вложение очищается)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)   // Этап выполнения операций целевого под-прохода
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)          // Операции целевого под-прохода
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)                // Синхронизация (по региону)
        );

        // Переход из основного во внешний (неявный)
        subpass_dependencies.push_back(
            vk::SubpassDependency()
            .setSrcSubpass(0)                                                     // Исходный под-проход (первый)
            .setDstSubpass(VK_SUBPASS_EXTERNAL)                                   // Целевой (внешний)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)   // Этап ожидания операций (вывод)
            .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)          // Операции записи
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)   // Этап выполнения операций целевого под-прохода
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead)           // Операции чтения (swap chain)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)                // Синхронизация (по региону)
        );

        // Создать проход
        vk_render_pass_ = vk_device_->logical_device().createRenderPassUnique(
            vk::RenderPassCreateInfo()
            .setAttachments(attachment_descriptions)
            .setSubpasses(subpass_descriptions)
            .setDependencies(subpass_dependencies));
    }

    void Renderer::init_vk_swap_chain(){
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        // Проверка формата поверхности
        const vk::SurfaceFormatKHR surface_format = {config_.color_format, config_.color_space};
        if (!vk_device_->supports_format(surface_format, vk_surface_)){
            throw std::runtime_error("Surface format is not supported by the device");
        }

        // Проверка поддержки нужного кол-ва изображений
        const auto surface_capabilities = vk_device_->physical_device().getSurfaceCapabilitiesKHR(*vk_surface_);
        if (config_.swap_chain_image_count > surface_capabilities.maxImageCount){
            throw std::runtime_error("Swap chain image count is greater than the maximum supported by the device");
        }

        // Проверка поддержки нужного режима представления (показа)
        const auto present_modes = vk_device_->physical_device().getSurfacePresentModesKHR(*vk_surface_);
        if (std::find(present_modes.begin(), present_modes.end(), config_.present_mode) == present_modes.end()){
            throw std::runtime_error("Present mode is not supported by the device");
        }

        // Старый swap-chain (может быть нужен в случае пере-создания)
        const vk::SwapchainKHR old_swap_chain = vk_swap_chain_ ? *vk_swap_chain_ : nullptr;

        // Индексы семейств очередей рендеринга и показа
        // На текущий момент выделяется одна группа очередей с поддержкой обеих команд (одно семейство)
        const std::vector family_indices = {
            vk_device_->queue_group(to<size_t>(CommandGroup::eGraphicsAndPresent)).family_index.value(),
        };

        // Используется ли одно и то же семейство для показа и рендеринга
        bool same_family = true;
        for (const auto& queue_family_index : family_indices){
            if (queue_family_index != family_indices[0]){
                same_family = false;
                break;
            }
        }

        // Инициализация swap chain
        auto create_info = vk::SwapchainCreateInfoKHR()
        .setSurface(vk_surface_.get())
        .setMinImageCount(config_.swap_chain_image_count)
        .setImageFormat(surface_format.format)
        .setImageColorSpace(surface_format.colorSpace)
        .setImageExtent(surface_capabilities.currentExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setImageSharingMode(same_family ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent)
        .setPreTransform(surface_capabilities.currentTransform)
        .setClipped(true)
        .setOldSwapchain(old_swap_chain);

        if (!same_family){
            create_info.setQueueFamilyIndices(family_indices);
        }

        // Если swap chain уже существует - лишить владения указатель
        if (vk_swap_chain_){
            vk_swap_chain_.release();
        }

        // Создать новый swap chain
        vk_swap_chain_ = vk_device_->logical_device().createSwapchainKHRUnique(create_info);

        // Уничтожить старый swap chain
        if (old_swap_chain){
            vk_device_->logical_device().destroySwapchainKHR(old_swap_chain);
        }
    }

    void Renderer::init_vk_framebuffers(){
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);
        assert(vk_swap_chain_);

        // Получить изображения swap chain
        const auto swap_chain_images = vk_device_->logical_device().getSwapchainImagesKHR(*vk_swap_chain_);
        assert(!swap_chain_images.empty());

        // Получить размеры кадрового буфера
        const auto swap_chain_extent = vk_device_->physical_device().getSurfaceCapabilitiesKHR(*vk_surface_).currentExtent;

        // Проход по изображениям swap-chain
        for (const auto& sci : swap_chain_images)
        {
            // Описать вложения кадрового буфера
            std::vector<vk::utils::Framebuffer::AttachmentInfo> attachments{};

            // Вложение цвета (используем изображение из swap-chain)
            vk::utils::Framebuffer::AttachmentInfo color{};
            color.image = sci;
            color.format = config_.color_format;
            color.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
            color.aspect = vk::ImageAspectFlagBits::eColor;
            attachments.push_back(color);

            // Для вложения глубины-трафарета изображения не создано (swap-chain создает только показываемые изображения)
            // НЕ указываем ничего в поле image (оно будет создано внутри кадрового буфера)
            vk::utils::Framebuffer::AttachmentInfo depth{};
            depth.format = config_.depth_stencil_format;
            depth.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
            depth.aspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
            attachments.push_back(depth);

            // Создать и добавить кадровый буфер
            vk_framebuffers_.emplace_back(std::make_unique<vk::utils::Framebuffer>(
                vk_device_,
                vk_render_pass_.get(),
                swap_chain_extent,
                attachments));
        }
    }

    void Renderer::init_vk_uniform_layouts(){
        assert(vk_instance_);
        assert(vk_device_);

        auto& layouts = vk_uniform_layouts_;
        layouts.resize(to<size_t>(UniformLayoutType::TOTAL));

        // Для шейдеров, которые не используют uniform блоки
        layouts[to<size_t>(UniformLayoutType::eDummy)] = std::make_unique<vk::utils::UniformLayout>(vk_device_);

        // Для шейдеров базовой растеризации (камера, объекты, текстуры)
        std::vector<vk::utils::UniformLayout::SetLayoutInfo> set_layouts = {
            // set = 0: Camera
            {
                {
                    {
                        0,
                        1,
                        vk::DescriptorType::eUniformBuffer,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                    }
                },
                1
            },
            // set = 1: Objects
            {
                {
                    // Матрицы трансформации всех объектов
                    {0,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex},
                },
                1
            },
            // set = 2: Material settings
            {
                {
                    // Параметры Phong материала для всех объектов
                    {0,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment},
                    // Параметры PBR материала для всех объектов
                    {1,1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment},
                },
                1
            },
            // set = 3: Material textures
            {
                {
                    // Текстуры albedo/diffuse для всех материалов (Phong/PBR материал)
                    {
                        to<uint32_t>(TextureType::eAlbedoColor),
                        MAX_MATERIALS,
                        vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment,
                        vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                    },
                    // Текстуры normal для всех материалов (Phong/PBR материал)
                    {
                        to<uint32_t>(TextureType::eNormal),
                        MAX_MATERIALS,
                        vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment,
                        vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                    },
                    // Текстуры roughness/specular для всех материалов (Phong/PBR материал)
                    {
                        to<uint32_t>(TextureType::eRoughnessOrSpecular),
                        MAX_MATERIALS,
                        vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment,
                        vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                    },
                    // Текстуры displace для всех материалов (Phong/PBR материал)
                    {
                        to<uint32_t>(TextureType::eHeight),
                        MAX_MATERIALS,
                        vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment,
                        vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                    },
                    // Текстуры metallic для всех материалов (Phong/PBR материал)
                    {
                        to<uint32_t>(TextureType::eMetallicOrReflection),
                        MAX_MATERIALS,
                        vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment,
                        vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                    },
                    // Текстуры ambient occlusion для всех материалов (PBR материал)
                    {
                        to<uint32_t>(TextureType::eAmbientOcclusion),
                        MAX_MATERIALS,
                        vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment,
                        vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                    },
                    // Текстуры emission для всех материалов (PBR материал)
                    {
                        to<uint32_t>(TextureType::eEmission),
                        MAX_MATERIALS,
                        vk::DescriptorType::eCombinedImageSampler,
                        vk::ShaderStageFlagBits::eFragment,
                        vk::DescriptorBindingFlagBitsEXT::ePartiallyBound
                    },
                },
                1
            },
            // set = 4: Light sources
            {
                {
                    // Источники света (параметры источников)
                    {
                        0,
                        1,
                        vk::DescriptorType::eStorageBuffer,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                    },
                    // Индексы источников
                    {
                        1,
                        1,
                        vk::DescriptorType::eStorageBuffer,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                    }
                },
                1
            }
        };

        // Push-константы layout'а растеризации
        std::vector push_constants{
            // Индекс объекта и индекс используемого материала
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment)
                .setSize(sizeof(uint32_t) * 2)
                .setOffset(0)
        };

        // Создать pipeline layout для растеризации
        layouts[to<size_t>(UniformLayoutType::eBasicRasterization)] = std::make_unique<vk::utils::UniformLayout>(
            vk_device_,
            set_layouts,
            push_constants);
    }

    void Renderer::init_vk_texture_samplers(){
        assert(vk_instance_);
        assert(vk_device_);

        auto& samplers = vk_texture_samplers_;
        const auto max_anisotropy = vk_device_->physical_device().getProperties().limits.maxSamplerAnisotropy;
        const bool anisotropy_supported = vk_device_->physical_device().getFeatures().samplerAnisotropy;

        // eNearest
        samplers[to<size_t>(TextureSamplerType::eNearest)] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eNearest)
                .setMagFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setAnisotropyEnable(false)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eNearestClamp
        samplers[to<size_t>(TextureSamplerType::eNearestClamp)] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eNearest)
                .setMagFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setAnisotropyEnable(false)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eLinear
        samplers[to<size_t>(TextureSamplerType::eLinear)] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eLinear)
                .setMagFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setAnisotropyEnable(false)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eLinearClamp
        samplers[to<size_t>(TextureSamplerType::eLinearClamp)] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eLinear)
                .setMagFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setAnisotropyEnable(false)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eAnisotropic
        samplers[to<size_t>(TextureSamplerType::eAnisotropic)] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eLinear)
                .setMagFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setAnisotropyEnable(anisotropy_supported)
                .setMaxAnisotropy(anisotropy_supported ? max_anisotropy : 1.0f)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );

        // eAnisotropicClamp
        samplers[to<size_t>(TextureSamplerType::eAnisotropicClamp)] =
            vk_device_->logical_device().createSamplerUnique(
                vk::SamplerCreateInfo()
                .setMinFilter(vk::Filter::eLinear)
                .setMagFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMinLod(0.0f)
                .setMaxLod(VK_LOD_CLAMP_NONE)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setAnisotropyEnable(anisotropy_supported)
                .setMaxAnisotropy(anisotropy_supported ? max_anisotropy : 1.0f)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
            );
    }

    void Renderer::init_vk_uniforms(){
        assert(vk_instance_);
        assert(vk_device_);

        // Получить необходимые объекты (пул, макеты наборов)
        const auto& ul = vk_uniform_layouts_[to<size_t>(UniformLayoutType::eBasicRasterization)];
        assert(ul);

        // Выделить дескрипторный набор для камеры
        ul->allocate_sets(0,1).front().swap(vk_dset_view_);
        // Выделить дескрипторный набор для uniform-буферов объектов (трансформации)
        ul->allocate_sets(1,1).front().swap(vk_dset_objects_uniforms_);
        // Выделить дескрипторный набор для uniform-буферов материалов (блики, шероховатость и прочее)
        ul->allocate_sets(2,1).front().swap(vk_dset_material_uniforms_);
        // Выделить дескрипторный набор для текстур материалов (по массиву дескрипторов на каждый вид текстур)
        ul->allocate_sets(3,1).front().swap(vk_dset_material_textures_);
        // Выделить дескрипторный набор для источников света
        ul->allocate_sets(4,1).front().swap(vk_dset_light_sources_);

        // Uniform буферы
        {
            // Выравнивание для uniform буферов
            const auto ubo_alignment = vk_device_->physical_device()
                .getProperties()
                .limits
                .minUniformBufferOffsetAlignment;

            // Выравнивание для storage буферов
            const auto sbo_alignment = vk_device_->physical_device()
                .getProperties()
                .limits
                .minStorageBufferOffsetAlignment;

            // Выделить uniform буфер для камеры (вид, проекция)
            vk_ubo_view_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(CameraUniforms), ubo_alignment),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для трансформаций объектов сцены
            vk_ubo_objects_transforms_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(ObjectTransformUniforms), sbo_alignment) * MAX_OBJECTS,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для параметров материала (Blin-Phong)
            vk_ubo_materials_phong_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(MaterialPhongUniforms), sbo_alignment) * MAX_MATERIALS,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для параметров материала (PBR)
            vk_ubo_materials_pbr_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(MaterialPbrUniforms), sbo_alignment) * MAX_MATERIALS,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для источников света
            vk_ubo_light_sources_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(LightUniforms), sbo_alignment) * MAX_OBJECTS,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            // Выделить uniform буфер для индексов источников
            vk_ubo_light_indices_ = std::make_unique<vk::utils::Buffer>(
                vk_device_,
                size_align(sizeof(LightIndices), sbo_alignment),
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }

        // Связать дескрипторы и буферы
        std::vector<vk::WriteDescriptorSet> writes;
        std::vector<vk::DescriptorBufferInfo> buffer_infos;
        buffer_infos.reserve(6); // ВНИМАНИЕ! Зарезервировать память перед использованием!
        writes.reserve(6);

        // Камера (set = 0, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_view_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(CameraUniforms)));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_view_.get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Трансформации объектов (set = 1, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_objects_transforms_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(ObjectTransformUniforms) * MAX_OBJECTS));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_objects_uniforms_.get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Параметры Phong материалов (set = 2, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_materials_phong_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(MaterialPhongUniforms) * MAX_MATERIALS));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_material_uniforms_.get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Параметры PBR материалов (set = 2, binding = 1)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_materials_pbr_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(MaterialPbrUniforms) * MAX_MATERIALS));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_material_uniforms_.get())
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Источники света (set = 4, binding = 0)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_light_sources_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(LightUniforms) * MAX_LIGHTS));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_light_sources_.get())
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Индексы активных источников света (set = 4, binding = 1)
        {
            buffer_infos.emplace_back(vk::DescriptorBufferInfo()
                .setBuffer(vk_ubo_light_indices_->vk_buffer())
                .setOffset(0)
                .setRange(sizeof(LightIndices)));

            writes.emplace_back(vk::WriteDescriptorSet()
                .setDstSet(vk_dset_light_sources_.get())
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&buffer_infos.back()));
        }

        // Связать дескрипторы с буферами
        vk_device_->logical_device().updateDescriptorSets(writes, {});

        // Подготовить uniform буферы к записи (разметка памяти)
        vk_ubo_view_->map_unsafe();
        vk_ubo_objects_transforms_->map_unsafe();
        vk_ubo_materials_phong_->map_unsafe();
        vk_ubo_materials_pbr_->map_unsafe();
        vk_ubo_light_sources_->map_unsafe();
        vk_ubo_light_indices_->map_unsafe();
    }

    void Renderer::init_vk_command_buffers(){
        assert(vk_instance_);
        assert(vk_device_);

        // Выделяем столько командных буферов, сколько предполагается активных кадров
        auto& pool = vk_device_->queue_group(to<size_t>(CommandGroup::eGraphicsAndPresent)).command_pools[0];
        vk_command_buffers_ = vk_device_->logical_device().allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo()
            .setCommandPool(pool.get())
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(config_.max_frames_in_flight));
    }

    void Renderer::init_vk_sync_objects(){
        assert(vk_instance_);
        assert(vk_device_);

        // Создать необходимые примитивы синхронизации для каждого активного кадра
        const auto& ld = vk_device_->logical_device();
        for (size_t i = 0; i < config_.max_frames_in_flight; ++i)
        {
            // Семафор, который будет ожидаться конвейером перед выполнением команд рендеринга
            vk_render_available_semaphore_.emplace_back(ld.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
            // Семафор, который будет сигнализировать о готовности к показу изображения (для команд показа)
            vk_render_finished_semaphore_.emplace_back(ld.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
            // Барьеры, которые показывают, что буфер был выполнен и готов к использованию
            vk_frame_fence_.emplace_back(ld.createFenceUnique(vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled}));
        }
    }

    void Renderer::init_index_pools(){
        object_ids_.reserve(MAX_OBJECTS);
        light_ids_.reserve(MAX_LIGHTS);
        active_light_ids_.reserve(MAX_LIGHTS);
        material_ids_.reserve(MAX_MATERIALS);
        materials_.reserve(MAX_MATERIALS);

        for (uint32_t i = MAX_OBJECTS; i > 0; --i){
            object_ids_.emplace_back(i - 1);
        }

        for (uint32_t i = MAX_LIGHTS; i > 0; --i){
            light_ids_.emplace_back(i - 1);
        }

        for (uint32_t i = MAX_MATERIALS; i > 0; --i){
            material_ids_.emplace_back(i - 1);
        }
    }

    void Renderer::refresh_vk_surface(){
        assert(vk_instance_);
        assert(vk_surface_);
        assert(vk_device_);

        // Ожидать завершения всех команд
        vk_device_->logical_device().waitIdle();

        // Отключить рендеринг и сбросить кадр
        is_rendering_ = false;
        current_frame_ = 0;

        // Очистить командные буферы
        vk_command_buffers_.clear();
        logger()->info("Vulkan: cleared command buffers");
        
        // Уничтожить кадровые буферы
        vk_framebuffers_.clear();
        logger()->info("Vulkan: cleared framebuffers");

        // Пере-создать swap-chain (старый будет задействован при создании нового, затем удален)
        init_vk_swap_chain();
        logger()->info("Vulkan: recreated swap chain");

        // Создать новые кадровые буферы
        init_vk_framebuffers();
        const auto extent = vk_framebuffers_[0]->extent();
        const std::string extent_str = std::to_string(extent.width) + "x" + std::to_string(extent.height);
        logger()->info("Vulkan: recreated framebuffers (" + extent_str + ")");

        // Создать новые командные буферы
        init_vk_command_buffers();
        logger()->info("Vulkan: recreated command buffers");

        // Включить рендеринг
        is_rendering_ = true;
    }
}
