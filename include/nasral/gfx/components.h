#pragma once

#include <nasral/core/types.h>
#include <nasral/core/component.h>
#include <nasral/gfx/types.h>

namespace nasral::gfx::comp
{
    /**
     * @brief Компонент хендлов экземпляра материала
     * @details Содержит хендлы конвейера и текстур материала
     */
    struct MaterialHandles : core::Component<MaterialHandles>
    {
        handles::Material material;
        core::EnumArray<TextureType, handles::Texture> textures;
    };

    /**
     * @brief Компонент параметров экземпляра материала
     * @details Содержит параметра материала, которые передаются в шейдер при обновлении
     */
    struct MaterialSettings : core::Component<MaterialSettings>
    {
        MaterialType type = MaterialType::eDummy;
        uniforms::Material uniforms = {};
        core::EnumArray<TextureType, TextureSamplerType> samplers;
    };

    /**
     * @brief Компонент хендлов меша
     * @details Содержит хендлы геометрических буферов меша и кол-во индексов
     */
    struct MeshHandles : core::Component<MeshHandles>
    {
        handles::Mesh mesh;
    };

    /**
     * @brief Компонент индекса
     * @details Содержит индекс элемента в буфере UBO/SSBO
     */
    struct UniformIndex : core::Component<UniformIndex>
    {
        uint32_t index = 0;
    };

    /**
     * @brief Компонент состояния данных для UBO/SSBO
     * @details Содержит флаг, показывающий необходимость обновления (для частых обновлений)
     */
    struct UniformState : core::Component<UniformState>
    {
        bool dirty = false;
    };

    /**
     * @brief Тег - нужно обновить UBO/SSBO
     * @details Для редких обновлений (статичные объекты сцены, материалы и прочее)
     */
    struct DirtyUniform : core::Component<DirtyUniform>
    {};

    /**
     * @brief Тег - нужно обновить дескрипторы текстур
     * @details Для редких обновлений (материалы)
     */
    struct DirtyTextures : core::Component<DirtyTextures>
    {};
}