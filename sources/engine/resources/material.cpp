#include "pch.h"
#include <pugixml.hpp>
#include <nasral/engine.h>
#include <nasral/resources/material.h>
#include <nasral/resources/shader.h>
#include <nasral/resources/resource_manager.h>


namespace nasral::resources
{
    Material::Material(ResourceManager* manager, const std::string_view& path)
        : path_(path)
        , vert_shader_res_(manager, Type::eShader, "")
        , frag_shader_res_(manager, Type::eShader, "")
        , vk_vert_shader_(std::nullopt)
        , vk_frag_shader_(std::nullopt)
    {
        resource_manager_ = SafeHandle<const ResourceManager>(manager);
        status_ = Status::eUnloaded;
        err_code_ = ErrorCode::eNoError;
        type_ = Type::eMaterial;
    }

    Material::~Material() = default;

    void Material::load() noexcept {
        const auto path = manager()->full_path(path_.data());

        pugi::xml_document doc;
        if (!doc.load_file(path.c_str())){
            status_ = Status::eError;
            err_code_ = ErrorCode::eLoadingError;
            return;
        }

        const auto shaders_conf = doc.child("Shaders");
        if (shaders_conf.empty()){
            status_ = Status::eError;
            err_code_ = ErrorCode::eBadFormat;
            return;
        }

        for (auto shader_info : shaders_conf.children("Shader")){
            const std::string shader_stage = shader_info.attribute("stage").as_string();
            const std::string shader_path = shader_info.attribute("path").as_string();

            if (shader_stage == "vertex"){
                vert_shader_res_.set_path(shader_path);
                vert_shader_res_.set_callback([this](IResource* resource){
                    const auto* v_shader = dynamic_cast<Shader*>(resource);
                    if (v_shader && v_shader->status() == Status::eLoaded){
                        vk_vert_shader_ = v_shader->vk_shader_module();
                        try_init_vk_objects();
                    }
                });
                vert_shader_res_.request();
            }
            else if (shader_stage == "fragment"){
                frag_shader_res_.set_path(shader_path);
                frag_shader_res_.set_callback([this](IResource* resource){
                    const auto* f_shader = dynamic_cast<Shader*>(resource);
                    if (f_shader && f_shader->status() == Status::eLoaded){
                        vk_frag_shader_ = f_shader->vk_shader_module();
                        try_init_vk_objects();
                    }
                });
                frag_shader_res_.request();
            }
        }

    }

    void Material::try_init_vk_objects(){
        // Если не все обработчики были вызваны - выход
        if (!vert_shader_res_.is_handled() || !frag_shader_res_.is_handled()){
            return;
        }

        // Если после вызова обработчиков не все shaders загружены - ошибка и выход
        if (!vk_vert_shader_.has_value() || !vk_frag_shader_.has_value()){
            status_ = Status::eError;
            err_code_ = ErrorCode::eLoadingError;
            return;
        }

        // Получить renderer и устройство
        const auto* renderer = resource_manager_->engine()->renderer();
        const auto& vd = renderer->vk_device();

        /** 1. Макет конвейера **/

        // Описываем макет дескрипторных наборов.
        // Макет очень сильно зависит от кода shader'а и должен его учитывать.

        // Дескрипторный набор для вида/камеры
        std::array<vk::DescriptorSetLayoutBinding, 1> view_uniform_bindings = {
            {
                vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                .setDescriptorCount(1)
            }
        };

        vk_dset_layout_view_ = vd->logical_device().createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo()
        .setBindings(view_uniform_bindings));

        // Дескрипторный набор для объектов сцены
        std::array<vk::DescriptorSetLayoutBinding, 1> object_uniform_bindings = {
            {
                vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                .setDescriptorCount(1)
            }
        };

        vk_dset_layout_object_ = vd->logical_device().createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo()
        .setBindings(object_uniform_bindings));

        // Создать макет всего конвейера
        try{
            std::array<vk::DescriptorSetLayout, 2> dset_layouts = {
                vk_dset_layout_view_.get(),
                vk_dset_layout_object_.get()
            };

            vk_pipeline_layout_ = vd->logical_device().createPipelineLayoutUnique(
                vk::PipelineLayoutCreateInfo()
                .setSetLayouts(dset_layouts));
        }
        catch([[maybe_unused]] std::exception& e){
            status_ = Status::eError;
            err_code_ = ErrorCode::eVulkanError;
            return;
        }


        /** 2. Входные данные **/

        // Vulkan позволяет привязывать сразу несколько вершинных буферов.
        // Это может быть полезно в том случае, если буферы хранят разную информацию.
        std::array<vk::VertexInputBindingDescription, 1> vertex_input_bindings = {
            vk::VertexInputBindingDescription()
            .setBinding(0)
            .setStride(sizeof(rendering::Vertex))
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
            .setOffset(offsetof(rendering::Vertex, pos)),

            // Нормаль
            vk::VertexInputAttributeDescription()
            .setLocation(1)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(rendering::Vertex, normal)),

            // Текстурные координаты
            vk::VertexInputAttributeDescription()
            .setLocation(2)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(rendering::Vertex, uv)),

            // Цвет
            vk::VertexInputAttributeDescription()
            .setLocation(3)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32B32A32Sfloat)
            .setOffset(offsetof(rendering::Vertex, color))
        };

        // Описываем стадию входных данных
        vk::PipelineVertexInputStateCreateInfo vertex_input_state = {};
        vertex_input_state.setVertexBindingDescriptions(vertex_input_bindings);
        vertex_input_state.setVertexAttributeDescriptions(vertex_input_attributes);


        /** 3. Сборка примитивов **/

        // Указываем как нужно собирать вершины и используется ли перезапуск примитивов
        // Перезапуск примитивов позволяет останавливать сборку примитива, переходя к следующему при обработке спец-индекса.
        // Спец-индекс для индексов 16 бит - 0xFFFF и 0xFFFFFFFF для 32 бит соответственно.
        vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{};
        input_assembly_state.setTopology(::vk::PrimitiveTopology::eTriangleList);
        input_assembly_state.setPrimitiveRestartEnable(false);

        /** 4. программируемые стадии (shaders) **/

        // Описываем программируемые стадии конвейера
        std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages{
            // Вершинный шейдер
            vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setModule(vk_vert_shader_.value())
            .setPName("main"),

            // Фрагментный шейдер
            vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setModule(vk_frag_shader_.value())
            .setPName("main")
        };

        /** 5. View-port **/

        // Пространство в стиле OpenGL
        const auto extent = renderer->get_rendering_resolution();
        const bool gl_style = renderer->config().use_opengl_style;

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
        rasterizer_state.setPolygonMode(::vk::PolygonMode::eFill);           // Заполнять полигоны цветом
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
        try{
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
                .setLayout(vk_pipeline_layout_.get())
                .setRenderPass(renderer->vk_render_pass())
                .setSubpass(0));

            if (result.result != vk::Result::eSuccess){
                throw std::runtime_error("Failed to create graphics pipeline!");
            }

            vk_pipeline_ = std::move(result.value);
        }
        catch([[maybe_unused]] std::exception& e){
            status_ = Status::eError;
            err_code_ = ErrorCode::eVulkanError;
            return;
        }

        // Ресурс готов
        status_ = Status::eLoaded;
        err_code_ = ErrorCode::eNoError;
    }
}
