#pragma once

#include <bitset>
#include <nasral/common/utils.h>
#include <nasral/ecs/components.h>
#include <nasral/gfx/components.h>
#include <nasral/res/components.h>

namespace nasral::ecs
{
    /**
     * @brief Перечисление всех используемых компонентов
     */
    using ComponentTypes = std::tuple
    <
        // Общие
        ActivateComponent,
        DeactivateComponent,
        DestroyComponent,
        UidComponent,
        NameComponent,

        // Ресурсы
        res::IdComponent,
        res::IdListComponent<gfx::TextureType>,
        res::RequestComponent,
        res::ReleaseComponent,
        res::ErrorComponent,
        res::LoadedComponent,

        // Графика
        gfx::MaterialHandlesComponent,
        gfx::MaterialSettingsComponent,
        gfx::UniformIndexComponent,
        gfx::UniformStateComponent,
        gfx::DirtyUnformComponent,
        gfx::DirtyTexturesComponent
    >;

    // Проверка типов компонентов на соответствие требования (на этапе компиляции)
    static_assert(
        all_default_constructible_v<ComponentTypes>,
        "All components must be default constructible");

    /**
     * @brief Битовая маска компонентов (размер зависит от кол-ва возможных типов компонентов)
     */
    using ComponentMask = std::bitset<std::tuple_size_v<ComponentTypes>>;

    /**
     * @brief Вариант-вектор компонентов (std::variant<std::vector<Component>>)
     */
    using ComponentPool = to_variant_vector<ComponentTypes>;

    /**
     * @brief Получение ID компонента (соответствует индексу элемента в ComponentTypes)
     * @tparam T Тип компонента
     */
    template <typename T>
    constexpr size_t kComponentId = tuple_index<T, ComponentTypes>();

    /**
     * @brief Получить маску компонентов
     * @tparam Ts Типы компонентов
     * @return Битовая маска
     */
    template<typename... Ts>
    ComponentMask make_mask(){
        ComponentMask mask;
        ((mask.set(kComponentId<Ts>)), ...);
        return mask;
    }

    /**
     * @brief Получить маску компонентов (константный алиас)
     * @tparam Ts Типы компонентов
     */
    template<typename... Ts>
    inline const ComponentMask kMaskOf = make_mask<Ts...>();

    /**
     * @brief Конфигурация подсистемы ECS
     */
    struct Config
    {
        size_t max_entities = 1024;
    };
}
