#pragma once

#include <nasral/core/types.h>
#include <nasral/core/component.h>
#include <nasral/res/types.h>
#include <nasral/gfx/types.h>

namespace nasral::res::comp
{
    /**
     * @brief Компонент дескрипторов материала
     * @details Содержит идентификаторы ресурсов материала и текстур
     */
    struct MaterialDescriptors : core::Component<MaterialDescriptors>
    {
        core::UniqueId uid = {};
        ResourceId mat_res_id = 0;
        core::EnumArray<gfx::TextureType, ResourceId> tex_res_ids = {};
    };

    /**
     * @brief Тег - сигнализирует о том, что материал нужно запросить
     */
    struct MaterialRequest : core::Component<MaterialRequest>
    {};

    /**
     * @brief Тег - сигнализирует о том, что материал нужно освободить
     */
    struct MaterialRelease : core::Component<MaterialRelease>
    {};

    /**
     * @brief Тег - сигнализирует об ошибке загрузки материала или его под-ресурсов
     */
    struct MaterialError : core::Component<MaterialError>
    {};

    /**
     * @brief Тег - сигнализирует о том, что материал в очереди на удаление
     */
    struct MaterialPendingRemove : core::Component<MaterialPendingRemove>
    {};
}