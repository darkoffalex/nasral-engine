#include "pch.h"
#include "utils/fps_counter.hpp"
#include "utils/surface_provider.hpp"

#include <nasral/engine.h>

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr auto kWindowTitle = "Demo";

namespace nrl = nasral;
namespace res = nasral::resources;

/**
 * Точка входа
 * @param argc Кол-во аргументов
 * @param argv Аргументы
 * @return Код выхода
 */
int main([[maybe_unused]] int argc, [[maybe_unused]] const char * argv[])
{
    try
    {
        // Инициализация GLFW
        if (glfwInit() != GLFW_TRUE){
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Для Vulkan не нужны hints
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // Создать окно
        GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, kWindowTitle, nullptr, nullptr);
        if (window == nullptr){
            throw std::runtime_error("Failed to create GLFW window");
        }

        // Конфигурация
        nasral::Engine::Config config;
        {
            // Логирование
            config.log.file = "engine.log";
            config.log.console_out = true;

            // Ресурсы
            config.resources.content_dir = "../../content/";
            config.resources.initial_resources = {
                // Материал без текстуры (цветные вершины)
                { res::Type::eShader, "materials/vertex-colored/shader.vert.spv", std::nullopt},
                { res::Type::eShader, "materials/vertex-colored/shader.frag.spv", std::nullopt},
                { res::Type::eMaterial, "materials/vertex-colored/material.xml", std::nullopt},
                // Текстурированный (unlit) материал
                { res::Type::eShader, "materials/textured/shader.vert.spv", std::nullopt},
                { res::Type::eShader, "materials/textured/shader.frag.spv", std::nullopt},
                { res::Type::eMaterial, "materials/textured/material.xml", std::nullopt},
                // Phong материал
                { res::Type::eShader, "materials/phong/shader.vert.spv", std::nullopt},
                { res::Type::eShader, "materials/phong/shader.frag.spv", std::nullopt},
                { res::Type::eShader, "materials/phong/shader.geom.spv", std::nullopt},
                { res::Type::eMaterial, "materials/phong/material.xml", std::nullopt},
                // PBR материал
                { res::Type::eShader, "materials/pbr/shader.vert.spv", std::nullopt},
                { res::Type::eShader, "materials/pbr/shader.frag.spv", std::nullopt},
                { res::Type::eShader, "materials/pbr/shader.geom.spv", std::nullopt},
                { res::Type::eMaterial, "materials/pbr/material.xml", std::nullopt},
                // Mesh для теста (мяч)
                { res::Type::eMesh, "meshes/football/fb.obj", std::nullopt},
                { res::Type::eMesh, "meshes/football/fb_deflated.obj", std::nullopt},
                // Mesh для теста (стул)
                { res::Type::eMesh, "meshes/chair/chair.obj", std::nullopt},
                // Текстуры (мяч)
                { res::Type::eTexture, "textures/football/fb_diff_1k.png", std::nullopt},
                { res::Type::eTexture, "textures/football/fb_nor_gl_1k.png", std::nullopt},
                { res::Type::eTexture, "textures/football/fb_spec_1k.png", std::nullopt},
                { res::Type::eTexture, "textures/football/fb_rough_1k.png", std::nullopt},
                // Текстуры (стул)
                { res::Type::eTexture, "textures/chair/chair_ao_1k.png", std::nullopt},
                { res::Type::eTexture, "textures/chair/chair_diff_1k.png:v0", res::TextureLoadParams().set_srgb(false)},
                { res::Type::eTexture, "textures/chair/chair_diff_1k.png:v1", res::TextureLoadParams().set_srgb(true)},
                { res::Type::eTexture, "textures/chair/chair_metal_1k.png", std::nullopt},
                { res::Type::eTexture, "textures/chair/chair_nor_gl_1k.png", std::nullopt},
                { res::Type::eTexture, "textures/chair/chair_rough_1k.png", std::nullopt},
                { res::Type::eTexture, "textures/chair/chair_spec_1k.png", std::nullopt},
            };

            // Рендеринг
            config.rendering.app_name = "engine-demo";
            config.rendering.engine_name = "nasral-engine";
            config.rendering.surface_provider = std::make_shared<utils::GlfwSurfaceProvider>(window);
            config.rendering.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
            config.rendering.pfn_vk_get_proc_addr = glfwGetInstanceProcAddress;
            config.rendering.rendering_resolution = {}; // На текущий момент не используется
            config.rendering.color_format = vk::Format::eB8G8R8A8Unorm;
            config.rendering.depth_stencil_format = vk::Format::eD32SfloatS8Uint;
            config.rendering.color_space = vk::ColorSpaceKHR::eSrgbNonlinear;
            config.rendering.present_mode = vk::PresentModeKHR::eImmediate;
            config.rendering.use_opengl_style = true;
            config.rendering.use_validation_layers = true;
            config.rendering.max_frames_in_flight = 3;
            config.rendering.swap_chain_image_count = 4;
        }

        // Инициализировать движок
        nasral::Engine engine;
        if(!engine.initialize(config)){
            throw std::runtime_error("Failed to initialize engine.");
        }

        // Таймер
        utils::FpsCounter fps_counter;
        fps_counter.set_fps_refresh_fn([&window](const unsigned fps){
            std::string title = kWindowTitle;
            title.append(" (").append(std::to_string(fps)).append(" FPS)");
            glfwSetWindowTitle(window, title.c_str());
        });

        // Main loop
        while (!glfwWindowShouldClose(window))
        {
            // Опрос событий GLFW
            glfwPollEvents();

            // Обновление FPS счетчика
            fps_counter.update();

            // Обновление движка
            engine.update(fps_counter.delta());
        }

        // Завершение работы с движком
        engine.shutdown();

        // Завершение работы с GLFW
        glfwTerminate();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}