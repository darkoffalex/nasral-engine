# Основная цель сборки (объектная библиотека)
add_library(Engine OBJECT
        # База
        pch.h
        engine.cpp

        # Логирование
        log/logger.cpp

        # События
        evt/manager.cpp
        evt/objects/listener.cpp

#        # Ввод
#        inp/manager.cpp
#        inp/provider.cpp

        # ECS
        ecs/archetype.cpp
        ecs/manager.cpp
        ecs/utils.cpp

        # Среда выполнения
        run/manager.cpp

        # Ресурсы
        res/manager.cpp
        res/system.cpp
        res/objects/resource.cpp
        res/objects/file.cpp
        res/objects/project.cpp
        res/objects/shader.cpp
        res/objects/texture.cpp
        res/objects/mesh.cpp
        res/objects/material.cpp
        res/objects/project.cpp
        res/objects/scene.cpp

        # Рендеринг
        gfx/manager.cpp
        gfx/renderer.cpp
        gfx/renderer_init.cpp
        gfx/objects/material.cpp
        gfx/system.cpp
        gfx/utils.cpp

        # Сцена
        scn/manager.cpp
        scn/objects/node.cpp
        scn/objects/spatial.cpp
        scn/objects/camera.cpp
        scn/objects/mesh.cpp
        scn/system.cpp
#        scn/io/base_node.cpp
#        scn/io/spatial_node.cpp
#        scn/io/camera_node.cpp
#        scn/io/light_node.cpp
#        scn/io/mesh_node.cpp
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
        nlohmann_json::nlohmann_json
)
