#pragma once

#include <nasral/core/types.h>
#include <nasral/core/component.h>
#include <nasral/res/types.h>

namespace nasral::res::comp
{
    /**
     * @brief Уникальный идентификатор ассета
     * @details Используется для связи между сущностями при построении и сохранении сцены
     */
    struct AssetId : core::Component<AssetId>
    {
        core::UniqueId uid = {};
    };

    /**
     * @brief Дескриптор ресурса
     * @details Хранит идентификатор ресурса (используется для запроса/освобождения)
     */
    struct Descriptor : core::Component<Descriptor>
    {
        ResourceId res_id = 0;
    };

    /**
     * @brief Список дескрипторов ресурсов
     * @tparam E Тип перечисления
     * @details Хранит для каждого варианта перечисления по дескриптору ресурса
     */
    template <typename E>
    struct DescriptorList : core::Component<DescriptorList<E>>
    {
        core::EnumArray<E, ResourceId> res_ids = {};
    };

    /**
     * @brief Тег - требуется запрос ресурса
     */
    struct Request : core::Component<Request>
    {};

    /**
     * @brief Тег - требуется освобождение ресурса
     */
    struct Release : core::Component<Release>
    {};

    /**
     * @brief Тег - ошибка загрузки ресурса
     */
    struct Error : core::Component<Error>
    {};

    /**
     * @brief Тег - ресурс загружен и доступен
     */
    struct Loaded : core::Component<Loaded>
    {};

    /**
     * @brief Тег - требуется удаление ассета
     */
    struct PendingDelete : core::Component<PendingDelete>
    {};
}