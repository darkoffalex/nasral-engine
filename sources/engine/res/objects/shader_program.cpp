#include "pch.h"
#include <nasral/res/objects/shader_program.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    ShaderProgram::ShaderProgram(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader)
        : Resource(manager, id, Type::eShaderProgram)
        , program_type_(gfx::ShaderProgramType::eRasterization)
        , screen_fx_pass_type_(gfx::ScreenFxPassType::eFinal)
    {}

    ShaderProgram::~ShaderProgram()
    {
        vk_pipeline_.reset();
        RES_LOG_DESTRUCTION();
    }

    void ShaderProgram::load() noexcept
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
                throw std::runtime_error("Failed to load shader program file: " + full_path);
            }

            // Код шейдера
            const auto& shader_code = data.value().code;
            // Тип программы (rasterization, screen space, raytracing, compute, etc.)
            program_type_ = data.value().program_type;

            // Получить renderer и устройство
            const auto* renderer = subsystem()->engine()->gfx()->renderer();
            auto& vd = renderer->vk_device();

            // Создать шейдерный модуль
            auto vk_shader_module = vd.logical_device().createShaderModuleUnique(
                vk::ShaderModuleCreateInfo()
                .setCodeSize(shader_code.size() * sizeof(uint32_t))
                .setPCode(shader_code.data())
            );

            // Инициализация в зависимости от типа программы
            switch (program_type())
            {
            case gfx::ShaderProgramType::eRasterization:
            case gfx::ShaderProgramType::eScreenSpace:
            case gfx::ShaderProgramType::eScreenOutput:
                init_rasterization_pipeline(vk_shader_module.get(), data.value().entry_points);
                break;
            case gfx::ShaderProgramType::eRaytracing:
                init_raytracing_pipeline(vk_shader_module.get(), data.value().entry_points);
                break;
            case gfx::ShaderProgramType::eCompute:
                init_compute_pipeline(vk_shader_module.get(), data.value().entry_points);
                break;
            default:
                throw std::runtime_error("Invalid shader program type");
                break;
            }

        }
        catch (const std::exception& e){
            set_status(Status::eError);
            set_error(error() == Error::eNone ? loader_->error() : error());
            RES_LOG_ERROR(error(), e.what());
        }
    }

    void ShaderProgram::init_rasterization_pipeline(
        const vk::ShaderModule& module,
        const std::vector<Data::EntryPointInfo>& entry_points)
    {
        // Данный метод может только инициализировать конвейер растеризации и пост-обработки
        assert(program_type() == gfx::ShaderProgramType::eRasterization
            || program_type() == gfx::ShaderProgramType::eScreenSpace
            || program_type() == gfx::ShaderProgramType::eScreenOutput && "Invalid shader program type");

        // Получить рендерер
        const auto* renderer = subsystem()->engine()->gfx()->renderer();
        auto& vd = renderer->vk_device();

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

        // Входные данные вершин есть у конвейера растеризации
        if (program_type() == gfx::ShaderProgramType::eRasterization){
            vertex_input_state.setVertexBindingDescriptions(vertex_input_bindings);
            vertex_input_state.setVertexAttributeDescriptions(vertex_input_attributes);
        }

        /** 2. Сборка примитивов (для конвейера растеризации) **/

        // Указываем как нужно собирать вершины и используется ли перезапуск примитивов
        // Перезапуск примитивов позволяет останавливать сборку примитива, переходя к следующему при обработке спец-индекса.
        // Спец-индекс для индексов 16 бит - 0xFFFF и 0xFFFFFFFF для 32 бит соответственно.
        vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{};
        input_assembly_state.setTopology(::vk::PrimitiveTopology::eTriangleList);
        input_assembly_state.setPrimitiveRestartEnable(false);

        /** 3. программируемые стадии (shaders) **/
        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
        for (const auto& [stage, name] : entry_points)
        {
            shader_stages.emplace_back(
                vk::PipelineShaderStageCreateInfo()
                    .setStage(stage)
                    .setModule(module)
                    .setPName(name.c_str()));
        }

        /** 4. View-port **/

        // Пространство в стиле OpenGL
        const auto extent = renderer->rendering_resolution();
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
        rasterizer_state.setPolygonMode(vk::PolygonMode::eFill);             // Заполнять полигоны цветом
        rasterizer_state.setCullMode(::vk::CullModeFlagBits::eBack);         // Отбрасывание задних (обратных) граней
        rasterizer_state.setFrontFace(::vk::FrontFace::eClockwise);          // Передние грани задаются по часовой стрелке
        rasterizer_state.setLineWidth(1.0f);                                 // Ширина линий (при рисовании линиями)
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

        // Настройки
        vk::PipelineColorBlendStateCreateInfo color_blending_state{};
        color_blending_state.setLogicOpEnable(false);   // Битовые логические операции отключены

        // Параметры смешивания для цветовых вложений

        // Пост-обработка (рисуем цвет на квадрате, смешивать не нужно)
        if (program_type() != gfx::ShaderProgramType::eRasterization)
        {
            std::array<vk::PipelineColorBlendAttachmentState, 1> attachments_blend{};
            attachments_blend[0] = vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(false)
                .setColorWriteMask(
                    vk::ColorComponentFlagBits::eR |
                    vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB |
                    vk::ColorComponentFlagBits::eA);

            color_blending_state.setAttachments(attachments_blend);
        }
        // Обычная растеризация (смешиваем только первое вложение)
        else
        {
            std::array<vk::PipelineColorBlendAttachmentState, 3> attachments_blend{};

            // Цвет смешивается через альфа-канал
            attachments_blend[0] = vk::PipelineColorBlendAttachmentState()
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
                    .setDstAlphaBlendFactor(vk::BlendFactor::eZero);                     // Итоговая альфа - множитель 0

            // Нормали и яркие области не смешиваются
            attachments_blend[1] = vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(false)
                .setColorWriteMask(
                    vk::ColorComponentFlagBits::eR |
                    vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB |
                    vk::ColorComponentFlagBits::eA);

            attachments_blend[2] = vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(false)
                .setColorWriteMask(
                    vk::ColorComponentFlagBits::eR |
                    vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB |
                    vk::ColorComponentFlagBits::eA);

            color_blending_state.setAttachments(attachments_blend);
        }

        /** 7. Динамические состояния  **/

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
            // Создаем структуру Dynamic Rendering для конвейера
            const auto [color_formats, depth_format, stencil_format] = attachment_formats();
            const auto pipeline_rendering_create_info = vk::PipelineRenderingCreateInfo()
                .setColorAttachmentFormats(color_formats)
                .setDepthAttachmentFormat(depth_format)
                .setStencilAttachmentFormat(stencil_format);

            // Описываем конвейер
            auto info = vk::GraphicsPipelineCreateInfo()
                .setPNext(&pipeline_rendering_create_info)
                .setStages(shader_stages)
                .setPVertexInputState(&vertex_input_state)
                .setPInputAssemblyState(&input_assembly_state)
                .setPViewportState(&viewport_state)
                .setPRasterizationState(&rasterizer_state)
                .setPDepthStencilState(&depth_stencil_state)
                .setPMultisampleState(&multisampling_state)
                .setPColorBlendState(&color_blending_state)
                .setPDynamicState(&dynamic_states_info)
                .setLayout(renderer->find_corresponding_pipeline_layout(program_type()))
                .setRenderPass(VK_NULL_HANDLE)
                .setSubpass(0);

            // Создаем конвейер
            auto result = vd.logical_device().createGraphicsPipelineUnique({},info);

            if (result.result != vk::Result::eSuccess){
                throw std::runtime_error("Failed to create graphics pipeline!");
            }

            vk_pipeline_ = std::move(result.value);
        }
        catch([[maybe_unused]] std::exception& e){
            set_status(Status::eError);
            set_error(Error::eVulkanError);
            RES_LOG_ERROR(error_, e.what());
        }
    }

    void ShaderProgram::init_raytracing_pipeline(
        [[maybe_unused]] const vk::ShaderModule& module,
        [[maybe_unused]] const std::vector<Data::EntryPointInfo>& entry_points)
    {
        // TODO: Implement
    }

    void ShaderProgram::init_compute_pipeline(
        [[maybe_unused]] const vk::ShaderModule& module,
        [[maybe_unused]] const std::vector<Data::EntryPointInfo>& entry_points)
    {
        // TODO: Implement
    }

    ShaderProgram::RenderingAttachmentFormatInfo ShaderProgram::attachment_formats() const
    {
        RenderingAttachmentFormatInfo formats = {};

        // Описываем вложения с которыми будет работать конвейер в зависимости от типа шейдерной программы
        switch (program_type())
        {
            case gfx::ShaderProgramType::eRasterization:
            {
                // 3 цветовые вложения
                formats.color_formats = {
                    engine()->gfx()->config().offscreen_color_format, // location = 0: Color
                    engine()->gfx()->config().offscreen_color_format, // location = 1: Normal
                    engine()->gfx()->config().offscreen_color_format, // location = 2: Emissive
                };

                // 1 вложение глубины
                formats.depth_format = engine()->gfx()->config().offscreen_depth_format;
                break;
            }
            case gfx::ShaderProgramType::eScreenSpace:
            {
                // 1 цветовое вложение (внутренний цветовой формат)
                formats.color_formats = {
                    engine()->gfx()->config().offscreen_color_format
                };
                break;
            }
            case gfx::ShaderProgramType::eScreenOutput: {
                // 1 цветовое вложение (цветовой формат поверхности показа)
                formats.color_formats = {
                    engine()->gfx()->config().present_color_format
                };
                break;
            }
            default: {
                break;
            }
        }

        return formats;
    }
}
