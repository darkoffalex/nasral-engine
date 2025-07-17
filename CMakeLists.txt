# Версия CMake
cmake_minimum_required(VERSION 3.15...3.29)

# Включить управление типом связывания runtime-библиотеки для MSVC
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Название проекта/решения
project(NasralEngine)

# Стандарт С/С++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пути для обнаружения CMake модулей (например, загруженных через Conan)
set(CMAKE_PREFIX_PATH
        build/generators
        build/Debug/generators
        build/Release/generators)

set(CMAKE_MODULE_PATH
        build/generators
        build/Debug/generators
        build/Release/generators)

# Найти нужные пакеты
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(stb REQUIRED)
find_package(Vulkan REQUIRED)
find_package(pugixml REQUIRED)

# Заголовочные файлы проекта
include_directories(BEFORE include)

# Устанавливаем каталоги для бинарных файлов
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)

# Статическое связывание с runtime библиотекой
option(STATIC_RUNTIME "Use static runtime linking" OFF)

# Определить архитектуру/разрядность
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_NAME "x64")
    set(ARCH "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ARCH_NAME "x86")
    set(ARCH "32")
else()
    message(FATAL_ERROR "Unsupported architecture. Size of pointer is: ${CMAKE_SIZEOF_VOID_P}")
endif()

# Вспомогательная функция для конфигурации целей сборки (target)
# Функция также добавляет необходимые флаги целям сборки, может быть вызвана в под-проектах
function(add_default_configurations TARGET_NAME TARGET_BIN_NAME)
    # Имя выходного файла
    set_property(TARGET ${TARGET_NAME} PROPERTY OUTPUT_NAME "${TARGET_BIN_NAME}_${ARCH_NAME}")

    # Для MSVC компиляторов
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_definitions(${TARGET_NAME} PRIVATE "-DNOMINMAX")
        target_compile_options(${TARGET_NAME} PRIVATE /W3 /permissive- /fp:fast)
        if(STATIC_RUNTIME)
            set_property(TARGET ${TARGET_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        else()
            set_property(TARGET ${TARGET_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
        endif()

    # Для GNU/MinGW/Clang
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic -ffast-math -Wno-unknown-pragmas)
        if(STATIC_RUNTIME)
            target_link_options(${TARGET_NAME} PRIVATE
                    $<$<PLATFORM_ID:Windows>:-static>
                    $<$<NOT:$<PLATFORM_ID:Windows>>:-static-libgcc -static-libstdc++>
            )
        endif()

    # Для других компиляторов (не поддерживается)
    else()
        message(WARNING "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

# Добавить под-проекты
add_subdirectory(sources/engine)
add_subdirectory(sources/demo-app)