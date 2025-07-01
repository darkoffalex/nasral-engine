#include "pch.h"
#include "utils/fps_counter.hpp"

#include <nasral/engine.h>

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;
constexpr auto WINDOW_TITLE = "Demo";

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
        if (glfwInit() != GLFW_TRUE)
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Для Vulkan не нужны hints
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // Создать окно
        GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
        if (window == nullptr)
        {
            throw std::runtime_error("Failed to create GLFW window");
        }

        // Конфигурация движка
        nasral::EngineConfig config;
        config.log.file = "engine.log";
        config.log.console_out = true;
        config.resources.content_dir = "../../content/";

        // Инициализировать движок
        nasral::Engine engine;
        engine.initialize(config);

        // Сформировать список ресурсов (временно, для теста)
        auto* m = engine.context().resource_manager;
        m->add_unsafe(nasral::resources::Type::eFile, "textures/checkerboard.png");

        // TODO: Загрузка других ресурсов (тест)
        // TODO: Создать ссылки на ресурсы (тест)
        // TODO: Запросить ресурсы и обработать загрузку (тест)

        // Таймер
        utils::FpsCounter fps_counter;
        fps_counter.set_fps_refresh_fn([&window](const unsigned fps){
            std::string title = WINDOW_TITLE;
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