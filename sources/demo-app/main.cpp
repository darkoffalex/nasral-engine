#include "pch.h"
#include "utils/fps_counter.hpp"

#include <nasral/engine.h>
#include <nasral/resources/file.h>

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
constexpr auto kWindowTitle = "Demo";

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
        GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, kWindowTitle, nullptr, nullptr);
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