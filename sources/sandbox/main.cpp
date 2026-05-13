#include "pch.h"
#include "utils/fps_counter.hpp"
#include "utils/surface_provider.hpp"
// #include "utils/input_provider.hpp"

#include <nasral/engine.h>

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr auto kWindowTitle = "Sandbox";

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
        nasral::Config config{};
        {
            // Логирование
            config.log.file = "nasral.log";
            config.log.console = true;
            config.log.level = nasral::log::kLevelDev;

            // ECS
            config.ecs.max_entities = 1000;

            // Ресурсы
            config.res.content_dir = "../../content/";
            // Эти ресурсы будут добавлены в список по умолчанию
            config.res.initial_resources = {
                {nasral::res::Type::eProjectFile, "project.json", std::nullopt},
            };

            // Графика (Vulkan)
            config.gfx.app_name = "Nasral Sandbox";
            config.gfx.engine_name = "Nasral Engine";
            config.gfx.surface_provider = std::make_shared<utils::GlfwSurfaceProvider>(window);
            config.gfx.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            config.gfx.pfn_vk_get_proc_addr = glfwGetInstanceProcAddress;
            config.gfx.color_format = vk::Format::eB8G8R8A8Unorm;
            config.gfx.depth_format = vk::Format::eD32SfloatS8Uint;
            config.gfx.color_space = vk::ColorSpaceKHR::eSrgbNonlinear;
            config.gfx.present_mode = vk::PresentModeKHR::eImmediate;
            config.gfx.enable_validation_layers = true;
            config.gfx.opengl_compatible = true;
            config.gfx.max_frames_in_flight = 3;
            config.gfx.swap_chain_images = 4;

            // Ввод
            // config.inp.provider = std::make_shared<utils::GlfwInputProvider>(window);
            // config.inp.default_sensitivity = 1.0f;
        }

        // Инициализировать движок
        nasral::Engine engine(config);

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
        engine.finalize();

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