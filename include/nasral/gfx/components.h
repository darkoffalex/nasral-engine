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
        uint32_t index = 0;
        uniforms::Material uniforms = {};
        core::EnumArray<TextureType, TextureSamplerType> samplers;
    };

    /**
     * @brief Тег - сигнализирует о том, что текстуры изменились (нужно обновить дескрипторы)
     */
    struct MaterialDirtyTextures : core::Component<MaterialDirtyTextures>
    {};

    /**
     * @brief Тег - сигнализирует о том, что настройки изменились (нужно обновить SSBO)
     */
    struct MaterialDirtySettings : core::Component<MaterialDirtySettings>
    {};
}