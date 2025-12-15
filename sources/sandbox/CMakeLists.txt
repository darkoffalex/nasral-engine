# Основная цель сборки
add_executable(Sandbox
        pch.h
        main.cpp
        utils/fps_counter.hpp
        utils/surface_provider.hpp
)
add_default_configurations(Sandbox sandbox)

# Текущий каталог как каталог включаемых фйлов.
# Для более удобного подключения пред-компилированного заголовка
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Пред-компилированные заголовки
target_precompile_headers(Sandbox PRIVATE pch.h)

# Связь с объектной библиотекой движка
target_link_libraries(Sandbox PRIVATE Engine)

# Связка с нужными библиотеками
target_link_libraries(Sandbox PRIVATE
        glfw
        Vulkan::Vulkan
        glm::glm
        magic_enum::magic_enum
)