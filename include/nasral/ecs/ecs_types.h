#pragma once
#include <vector>
#include <bitset>
#include <variant>
#include <nasral/core_types.h>

#include <nasral/resources/resource_components.h>
#include <nasral/rendering/rendering_components.h>

namespace nasral::ecs
{
    using ComponentTypes = std::tuple<
        // Ресурсы (дескрипторы и запросы)
        resources::components::MaterialRequest,       // 0
        resources::components::MaterialDescriptors,   // 1

        // Рендеринг (handles)
        rendering::components::MaterialHandles,       // 3
        rendering::components::MaterialSettings       // 4
    >;

#pragma region meta_magic_componenet_index
    template <typename T, typename Tuple>
    struct tuple_index;

    template <typename T, typename... Rest>
    struct tuple_index<T, std::tuple<T, Rest...>> {
        static constexpr size_t value = 0;
    };

    template <typename T, typename U, typename... Rest>
    struct tuple_index<T, std::tuple<U, Rest...>> {
        static constexpr size_t value = 1 + tuple_index<T, std::tuple<Rest...>>::value;
    };

    template <typename T>
    struct tuple_index<T, std::tuple<>> {
        static_assert(sizeof(T) == 0, "Type not found in tuple");
    };
#pragma endregion

    template <typename T>
    constexpr size_t kComponentId = tuple_index<T, ComponentTypes>::value;

#pragma region meta_magic_pool_variants
    template <typename Tuple, typename IndexSeq>
    struct tuple_to_vector_variant_impl;

    template <typename Tuple, std::size_t... Is>
    struct tuple_to_vector_variant_impl<Tuple, std::index_sequence<Is...>> {
        using type = std::variant<std::vector<std::tuple_element_t<Is, Tuple>>...>;
    };

    template <typename Tuple>
    using tuple_to_vector_variant = typename tuple_to_vector_variant_impl<
        Tuple,
        std::make_index_sequence<std::tuple_size_v<Tuple>>
    >::type;
#pragma endregion

    using ComponentPoolVariant = tuple_to_vector_variant<ComponentTypes>;

    using ComponentMask = std::bitset<std::tuple_size_v<ComponentTypes>>;

    template<typename... Ts>
    ComponentMask make_mask(){
        ComponentMask mask;
        ((mask.set(kComponentId<Ts>)), ...);
        return mask;
    }

    template<typename... Ts>
    const ComponentMask kMaskOf = make_mask<Ts...>();

    struct EntityId
    {
        size_t index = 0;
        size_t version = 0;
        bool operator==(const EntityId& other) const{
            return index == other.index && version == other.version;
        }
    };

    struct EcsConfig
    {
        size_t max_entities = 1000;
    };

    class EcsError final : public EngineError
    {
    public:
        explicit EcsError(const std::string& message)
        : EngineError(message) {}
    };
}