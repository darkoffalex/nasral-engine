# Основная цель сборки (объектная библиотека)
add_library(Engine OBJECT
        # База
        pch.h
        engine.cpp

        # Логирование
        logging/logger.cpp

        # Ресурсы
        resources/ref.cpp
        resources/file.cpp
        resources/shader.cpp
        resources/material.cpp
        resources/mesh.cpp
        resources/texture.cpp
        resources/loaders/shader_loader.hpp
        resources/loaders/material_loader.hpp
        resources/loaders/mesh_loader.hpp
        resources/loaders/mesh_builtin_loader.hpp
        resources/loaders/texture_loader.hpp
        resources/loaders/texture_builtin_loader.hpp
        resources/resource_manager.cpp

        # Рендеринг
        rendering/renderer.cpp
        rendering/material_instance.cpp
        rendering/mesh_instance.cpp
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
        Vulkan::Vulkan
)