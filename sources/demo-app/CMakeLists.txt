# Основная цель сборки
add_executable(DemoApp
        pch.h
        main.cpp
        utils/fps_counter.hpp
)
add_default_configurations(DemoApp demo_app)

# Текущий каталог как каталог включаемых фйлов.
# Для более удобного подключения пред-компилированного заголовка
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Пред-компилированные заголовки
target_precompile_headers(DemoApp PRIVATE pch.h)

# Связь с объектной библиотекой движка
target_link_libraries(DemoApp PRIVATE Engine)

# Связка с нужными библиотеками
target_link_libraries(DemoApp PRIVATE
        glfw
        Vulkan::Vulkan
)