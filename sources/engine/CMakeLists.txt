# Основная цель сборки (объектная библиотека)
file(GLOB_RECURSE ENGINE_SOURCES *.cpp)
add_library(Engine OBJECT ${ENGINE_SOURCES})
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