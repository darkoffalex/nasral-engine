# Основная цель сборки (объектная библиотека)
add_library(Engine OBJECT
        # База
        pch.h
        engine.cpp

        # Логирование
        log/logger.cpp

        # ECS
        ecs/archetype.cpp
        ecs/manager.cpp

        # Ресурсы
        res/manager.cpp
        res/resources/file.cpp
        res/resources/shader.cpp
        res/resources/texture.cpp
        res/resources/mesh.cpp
        res/resources/material.cpp

        # Рендеринг
        gfx/renderer.cpp
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
        pugixml::pugixml
        assimp::assimp
        Vulkan::Vulkan
        magic_enum::magic_enum
)