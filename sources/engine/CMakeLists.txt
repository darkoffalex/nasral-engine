# Основная цель сборки (объектная библиотека)
add_library(Engine OBJECT
        pch.h
        engine.cpp
        logging/logger.cpp
        resources/ref.cpp
        resources/file.cpp
        resources/resource_manager.cpp
        rendering/renderer.cpp
)
add_default_configurations(Engine demo_app)

# Текущий каталог как каталог включаемых фйлов.
# Для более удобного подключения пред-компилированного заголовка
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Пред-компилированные заголовки
target_precompile_headers(Engine PRIVATE pch.h)

# Связка с нужными библиотеками
target_link_libraries(Engine PRIVATE
        glfw
        imgui::imgui
        glm::glm
        stb::stb
        Vulkan::Vulkan
)