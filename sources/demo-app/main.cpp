#include "pch.h"
#include "utils/fps_counter.hpp"
#include "utils/surface_provider.hpp"

#include <nasral/engine.h>

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
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
                { nasral::resources::Type::eShader, "materials/triangle/shaders/vertex.spv"},
                { nasral::resources::Type::eShader, "materials/triangle/shaders/fragment.spv"},
                { nasral::resources::Type::eMaterial, "materials/triangle/material.xml"}
            };

            // Рендеринг
            config.rendering.app_name = "engine-demo";
            config.rendering.engine_name = "nasral-engine";
            config.rendering.surface_provider = std::make_shared<utils::GlfwSurfaceProvider>(window);
            config.rendering.clear_color = {1.0f, 0.0f, 0.0f, 1.0f};
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

        // Тестовые ресурсы (только для тестирования)
        auto* rm = engine.resource_manager();
        auto vsr = rm->make_ref(res::Type::eShader, "materials/triangle/shaders/vertex.spv");
        auto fsr = rm->make_ref(res::Type::eShader, "materials/triangle/shaders/fragment.spv");
        auto mtr = rm->make_ref(res::Type::eMaterial, "materials/triangle/material.xml");

        // Запрос vertex shader (отдельно)
        /*
        vsr.set_callback([](const res::IResource* res){
            if (res->status() == res::Status::eLoaded){
                std::cout << "Vertex shader resource loaded!" << std::endl;
            }
        });
        vsr.request();

        // Запрос fragment shader (отдельно)
        fsr.set_callback([](const res::IResource* res){
            if (res->status() == res::Status::eLoaded){
                std::cout << "Fragment shader resource loaded!" << std::endl;
            }
        });
        fsr.request();
        */

        // Запрос материала (внутри запрашивает shaders)
        mtr.set_callback([&](const res::IResource* res){
            if (res->status() == res::Status::eLoaded){
                std::cout << "Material resource loaded!" << std::endl;
                const auto vc = rm->ref_count("materials/triangle/shaders/vertex.spv");
                const auto fc = rm->ref_count("materials/triangle/shaders/fragment.spv");
                std::cout << "Ref count: " << vc << ", " << fc << std::endl;
            }
        });
        mtr.request();

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

        // Освободить ресурсы
        vsr.release();
        fsr.release();
        mtr.release();

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