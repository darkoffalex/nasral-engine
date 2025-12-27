#include <nasral/res/resources/material.h>
#include <nasral/res/resources/shader.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Material::Material(Manager* manager, const ResourceId id, Loader<Data>::Ptr loader)
        : IResource(Type::eMaterial, id, manager)
        , loader_(std::move(loader))
        , material_type_(gfx::MaterialType::eDummy)
        , vk_polygon_mode_(vk::PolygonMode::eFill)
        , vk_line_width_(0.0f)
        , vert_shader_id_(std::nullopt)
        , frag_shader_id_(std::nullopt)
        , geom_shader_id_(std::nullopt)
        , vk_vert_shader_(std::nullopt)
        , vk_frag_shader_(std::nullopt)
        , vk_geom_shader_(std::nullopt)
        , base_shd_loads_needed_(0)
        , geom_shd_loads_needed_(0)
    {}

    Material::~Material(){
        release_all_sub_resources();
        vk_pipeline_.reset();
        RES_LOG_DESTRUCTION();
    }

    void Material::load() noexcept
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
                throw std::runtime_error("Failed to load material: " + path);
            }

            // Параметры материала (используются при инициализации vulkan pipeline)
            material_type_ = data->type;
            vk_polygon_mode_ = data->polygon_mode;
            vk_line_width_ = std::max(data->line_width, 1.0f);

            // Базовые шейдеры всегда нужны
            assert(!data->vert_shader_path.empty() && !data->frag_shader_path.empty());
            if (!data->vert_shader_path.empty() || !data->frag_shader_path.empty())
            {
                // Получить ID ресурсов по путям
                vert_shader_id_ = manager()->find(data->vert_shader_path);
                frag_shader_id_ = manager()->find(data->frag_shader_path);
                if (!vert_shader_id_.has_value() || !frag_shader_id_.has_value()){
                    status_ = Status::eError;
                    error_ = Error::eLoadingFailed;
                    throw std::runtime_error("Failed find vertex or fragment shader resources for material: " + path);
                }

                // Осталось загрузить 2 базовых шейдера
                base_shd_loads_needed_.store(2, std::memory_order_release);
            }

            // Геометрический шейдер может быть опциональным
            if (!data->geom_shader_path.empty())
            {
                // Получить ID ресурса по путям
                geom_shader_id_ = manager()->find(data->geom_shader_path);
                if (!geom_shader_id_.has_value()){
                    status_ = Status::eError;
                    error_ = Error::eLoadingFailed;
                    throw std::runtime_error("Failed find geometry shader resources for material: " + path);
                }

                geom_shd_loads_needed_.store(1, std::memory_order_release);
            }

            // Запрос под-ресурса вершинного шейдера
            // После завершения ПОПЫТКИ загрузки вызовет try_init_vk_pipeline()
            manager()->request(vert_shader_id_.value(), [this](IResource* resource){
                const auto* shader = dynamic_cast<Shader*>(resource);
                if (shader && shader->status() == Status::eLoaded){
                    vk_vert_shader_ = shader->vk_shader_module();
                }

                base_shd_loads_needed_.fetch_sub(1, std::memory_order_acq_rel);
                try_init_vk_pipeline();
            });

            // Запрос под-ресурса фрагментного шейдера
            // После завершения ПОПЫТКИ загрузки вызовет try_init_vk_pipeline()
            manager()->request(frag_shader_id_.value(), [this](IResource* resource){
                const auto* shader = dynamic_cast<Shader*>(resource);
                if (shader && shader->status() == Status::eLoaded){
                    vk_frag_shader_ = shader->vk_shader_module();
                }

                base_shd_loads_needed_.fetch_sub(1, std::memory_order_acq_rel);
                try_init_vk_pipeline();
            });

            // Опционально - запрос под-ресурсы геометрического shader-а
            // После завершения ПОПЫТКИ загрузки вызовет try_init_vk_pipeline()
            if (geom_shader_id_.has_value()){
                manager()->request(geom_shader_id_.value(), [this](IResource* resource){
                    const auto* shader = dynamic_cast<Shader*>(resource);
                    if (shader && shader->status() == Status::eLoaded){
                        vk_geom_shader_ = shader->vk_shader_module();
                    }

                    geom_shd_loads_needed_.fetch_sub(1, std::memory_order_acq_rel);
                    try_init_vk_pipeline();
                });
            }
        }
        catch (const std::exception& e)
        {
            status_ = Status::eError;
            error_ = error_ == Error::eNone ? Error::eLoadingFailed : error_;
            RES_LOG_ERROR(error_, e.what());
        }
    }

    void Material::release_all_sub_resources()
    {
        if (vert_shader_id_.has_value()) manager()->release(vert_shader_id_.value());
        if (frag_shader_id_.has_value()) manager()->release(frag_shader_id_.value());
        if (geom_shader_id_.has_value()) manager()->release(geom_shader_id_.value());
        vert_shader_id_ = std::nullopt;
        frag_shader_id_ = std::nullopt;
        geom_shader_id_ = std::nullopt;
    }

    void Material::try_init_vk_pipeline()
    {
        // Если не все обязательные шейдеры запрошены - выход (ожидаем другого вызова)
        if (base_shd_loads_needed_.load(std::memory_order_acquire) > 0){
            return;
        }

        // Если были запросы не обязательных стадий, но они не выполнены - выход
        if (geom_shader_id_.has_value() && geom_shd_loads_needed_.load(std::memory_order_acquire) > 0){
            return;
        }

        // Если не все обязательные шейдеры в итоге успешно загружены
        if (!vk_vert_shader_.has_value() || !vk_frag_shader_.has_value()){
            status_ = Status::eError;
            error_ = Error::eLoadingFailed;
            RES_LOG_ERROR(error_, "Failed to load vertex or fragment shader resources for material: " + manager()->path(id_));
            return;
        }

        // Если не обязательный шейдер был запрошен, но не загружен
        if (geom_shader_id_.has_value() && !vk_geom_shader_.has_value()){
            status_ = Status::eError;
            error_ = Error::eLoadingFailed;
            RES_LOG_ERROR(error_, "Failed to load geometry shader resources for material: " + manager()->path(id_));
            return;
        }

        // Получить renderer и устройство
        const auto* renderer = manager_->engine()->renderer();
        const auto& ul = renderer->vk_uniform_layout(gfx::UniformLayoutType::eBasicRasterization);
        const auto& vd = renderer->vk_device_ptr();

        /** 1. Входные данные **/

        // Vulkan позволяет привязывать сразу несколько вершинных буферов.
        // Это может быть полезно в том случае, если буферы хранят разную информацию.
        std::array<vk::VertexInputBindingDescription, 1> vertex_input_bindings = {
            vk::VertexInputBindingDescription()
            .setBinding(0)
            .setStride(sizeof(gfx::Vertex))
            .setInputRate(vk::VertexInputRate::eVertex)
        };

        // Описываем атрибуты вершин
        // Для каждого атрибута можно указать, к какому буферу он относится (binding)
        std::array<vk::VertexInputAttributeDescription, 4> vertex_input_attributes = {
            // Положение
            vk::VertexInputAttributeDescription()
            .setLocation(0)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(gfx::Vertex, pos)),

            // Нормаль
            vk::VertexInputAttributeDescription()
            .setLocation(1)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(gfx::Vertex, normal)),

            // Текстурные координаты
            vk::VertexInputAttributeDescription()
            .setLocation(2)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(gfx::Vertex, uv)),

            // Цвет
            vk::VertexInputAttributeDescription()
            .setLocation(3)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32B32A32Sfloat)
            .setOffset(offsetof(gfx::Vertex, color))
        };

        // Описываем стадию входных данных
        vk::PipelineVertexInputStateCreateInfo vertex_input_state = {};
        vertex_input_state.setVertexBindingDescriptions(vertex_input_bindings);
        vertex_input_state.setVertexAttributeDescriptions(vertex_input_attributes);


        /** 2. Сборка примитивов **/

        // Указываем как нужно собирать вершины и используется ли перезапуск примитивов
        // Перезапуск примитивов позволяет останавливать сборку примитива, переходя к следующему при обработке спец-индекса.
        // Спец-индекс для индексов 16 бит - 0xFFFF и 0xFFFFFFFF для 32 бит соответственно.
        vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{};
        input_assembly_state.setTopology(::vk::PrimitiveTopology::eTriangleList);
        input_assembly_state.setPrimitiveRestartEnable(false);

        /** 3. программируемые стадии (shaders) **/

        // Описываем программируемые стадии конвейера
        assert(vk_vert_shader_.has_value());
        assert(vk_frag_shader_.has_value());
        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
        shader_stages.reserve(3);

        // Вершинный шейдер
        shader_stages.emplace_back(
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eVertex)
                .setModule(vk_vert_shader_.value())
                .setPName("main"));

        // Фрагментный шейдер
        shader_stages.emplace_back(
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eFragment)
                .setModule(vk_frag_shader_.value())
                .setPName("main"));

        // Геометрический шейдер (если нужно)
        if (vk_geom_shader_.has_value()){
            shader_stages.emplace_back(
                vk::PipelineShaderStageCreateInfo()
                    .setStage(vk::ShaderStageFlagBits::eGeometry)
                    .setModule(vk_geom_shader_.value())
                    .setPName("main"));
        }

        /** 4. View-port **/

        // Пространство в стиле OpenGL
        const auto extent = renderer->get_rendering_resolution();
        const bool gl_style = renderer->config().opengl_compatible;

        // Статическая настройка вью-портов (может быть несколько)
        std::array<vk::Viewport, 1> viewports = {
            vk::Viewport()
            .setX(0.0f)
            .setY(static_cast<float>(gl_style ? extent.height : 0))
            .setWidth(static_cast<float>(extent.width))
            .setHeight(gl_style ? -static_cast<float>(extent.height) : static_cast<float>(extent.height))
            .setMinDepth(0.0f)
            .setMaxDepth(1.0f)
        };

        // Обрезка/ножницы (может быть несколько)
        std::array<vk::Rect2D, 1> scissors = {
            vk::Rect2D()
            .setOffset({0, 0})
            .setExtent(extent)
        };

        // Описываем view-port и обрезку
        vk::PipelineViewportStateCreateInfo viewport_state{};
        viewport_state.setScissors(scissors);
        viewport_state.setViewports(viewports);

        /** 5. Растеризация  **/

        // Основное
        vk::PipelineRasterizationStateCreateInfo rasterizer_state{};
        rasterizer_state.setRasterizerDiscardEnable(false);                  // Не пропускать этап rasterization
        rasterizer_state.setPolygonMode(vk_polygon_mode_);                   // Заполнять полигоны цветом
        rasterizer_state.setCullMode(::vk::CullModeFlagBits::eBack);         // Отбрасывание задних (обратных) граней
        rasterizer_state.setFrontFace(::vk::FrontFace::eClockwise);          // Передние грани задаются по часовой стрелке
        rasterizer_state.setLineWidth(vk_line_width_);                       // Ширина линий (при рисовании линиями)
        rasterizer_state.setDepthClampEnable(false);                         // Ограничивать значения глубины выходящими за диапазон
        rasterizer_state.setDepthBiasEnable(false);                          // Смещение для глубины (полезно в shadow mapping)
        rasterizer_state.setDepthBiasConstantFactor(0.0f);                   // Постоянное смещение (полезно в shadow mapping)
        rasterizer_state.setDepthBiasSlopeFactor(0.0f);                      // Смещение для наклонных пов-тей.
        rasterizer_state.setDepthBiasClamp(0.0f);                            // Ограничить смещение

        // Тест глубины
        vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{};
        depth_stencil_state.setDepthTestEnable(true);                          // Включить тест глубины
        depth_stencil_state.setDepthWriteEnable(true);                         // Включить запись во вложения глубины
        depth_stencil_state.setDepthCompareOp(::vk::CompareOp::eLessOrEqual);  // Сравнение "меньше ли равно"
        depth_stencil_state.setDepthBoundsTestEnable(false);                   // Отбрасывать фрагменты вне диапазона
        depth_stencil_state.setStencilTestEnable(false);                       // Не использовать тест трафарета

        // Multisampling
        vk::PipelineMultisampleStateCreateInfo multisampling_state{};
        multisampling_state.setSampleShadingEnable(false);
        multisampling_state.setRasterizationSamples(::vk::SampleCountFlagBits::e1);
        multisampling_state.setMinSampleShading(1.0f);
        multisampling_state.setAlphaToCoverageEnable(false);
        multisampling_state.setAlphaToOneEnable(false);

        /** 6. Смешивание цветов (прозрачность и наложение)  **/

        // Параметры смешивания для цветовых вложений
        std::array<vk::PipelineColorBlendAttachmentState, 1> attachments_blend{
            vk::PipelineColorBlendAttachmentState()
            .setBlendEnable(true)                                                // Включить смешивание
            .setColorWriteMask(
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA)                                  // Запись в 4 канала
            .setColorBlendOp(vk::BlendOp::eAdd)                                  // Аддитивное смешивание (цвет)
            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)                  // Исходный цвет - альфа текущего цвета
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)          // Итоговый цвет - 1 - альфа исходного
            .setAlphaBlendOp(vk::BlendOp::eAdd)                                  // Аддитивное смешивание (альфа)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)                       // Исходная альфа - множитель 1
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)                      // Итоговая альфа - множитель 0
        };

        // Настройки
        vk::PipelineColorBlendStateCreateInfo color_blending_state{};
        color_blending_state.setAttachments(attachments_blend);
        color_blending_state.setLogicOpEnable(false);                           // Битовые логические операции отключены

        /** 7. Смешивание цветов (прозрачность и наложение)  **/

        // Какие из состояния могут изменяться динамически (посредством команд)
        std::array<vk::DynamicState, 2> dynamic_states{
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dynamic_states_info{};
        dynamic_states_info.setDynamicStates(dynamic_states);

        /** 8. Конвейер **/

        // Попытка инициализации графического конвейера
        try
        {
            if (!ul.vk_pipeline_layout()){
                throw std::runtime_error("Can't create graphics pipeline. Pipeline layout is not initialized!");
            }

            auto result = vd->logical_device().createGraphicsPipelineUnique(
                {},
                vk::GraphicsPipelineCreateInfo()
                .setStages(shader_stages)
                .setPVertexInputState(&vertex_input_state)
                .setPInputAssemblyState(&input_assembly_state)
                .setPViewportState(&viewport_state)
                .setPRasterizationState(&rasterizer_state)
                .setPDepthStencilState(&depth_stencil_state)
                .setPMultisampleState(&multisampling_state)
                .setPColorBlendState(&color_blending_state)
                .setPDynamicState(&dynamic_states_info)
                .setLayout(ul.vk_pipeline_layout())
                .setRenderPass(renderer->vk_render_pass())
                .setSubpass(0));

            if (result.result != vk::Result::eSuccess){
                throw std::runtime_error("Failed to create graphics pipeline!");
            }

            vk_pipeline_ = std::move(result.value);
        }
        catch([[maybe_unused]] std::exception& e){
            status_ = Status::eError;
            error_ = Error::eVulkanError;
            RES_LOG_ERROR(error_, e.what());
            return;
        }

        // Освободить суб-ресурсы и обнулить их
        release_all_sub_resources();

        // Ресурс готов
        status_ = Status::eLoaded;
        error_ = Error::eNone;
        RES_LOG_LOADED();
    }
}
