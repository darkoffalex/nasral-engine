#pragma once

#include <bitset>
#include <nasral/ecs/utils.h>
#include <nasral/core/component.h>

namespace nasral::ecs::components
{
    struct Dummy1 : core::Component<Dummy1>
    {
        float a = 0;
        float b = 0;
        float c = 0;
    };

    struct Dummy2 : core::Component<Dummy2>
    {
        float a = 0;
        float b = 0;
        float c = 0;
    };
}

namespace nasral::ecs
{
    // Tuple из возможных типов компонентов
    using ComponentTypes = std::tuple<
        components::Dummy1,
        components::Dummy2
    >;

    // Битовая маска компонентов (размер зависит от кол-ва возможных типов компонентов)
    using ComponentMask = std::bitset<std::tuple_size_v<ComponentTypes>>;

    // Вариант-вектор компонентов
    using ComponentPool = to_variant_vector<ComponentTypes>;

    // Получение ID компонента (соответствует индексу элемента в ComponentTypes)
    template <typename T>
    constexpr size_t kComponentId = tuple_index<T, ComponentTypes>::value;

    // Получение битовой маски типу компонентов
    template<typename... Ts>
    constexpr auto kMaskOf = [] {
        ComponentMask mask;
        ((mask.set(kComponentId<Ts>)), ...);
        return mask;
    }();

    struct Config
    {
        size_t max_entities = 1024;
    };
}