#pragma once

#include <tuple>
#include <variant>
#include <vector>

namespace nasral::ecs
{
    /**
     * Конвертация std::tuple<T1, T2..> в
     * std::variant<std::vector<T1>, std::vector<T2>..>
     */
#pragma region meta_magic_variant_vector
    // Основной шаблон.
    template <typename Tuple, typename IndexSeq>
    struct to_variant_vector_impl;

    // Частичная специализация (для параметров типа Tuple и std::index_sequence<Is...>>)
    template <typename Tuple, std::size_t... Is>
    struct to_variant_vector_impl<Tuple, std::index_sequence<Is...>> {
        using type = std::variant<std::vector<std::tuple_element_t<Is, Tuple>>...>;
    };

    // Alias для удобства
    template <typename Tuple>
    using to_variant_vector = typename to_variant_vector_impl<
        Tuple,
        std::make_index_sequence<std::tuple_size_v<Tuple>>
    >::type;
#pragma endregion


    /**
     * Получение индекса типа в std::tuple
     */
#pragma region meta_magic_tuple_index
    // Основной шаблон (декларация).
    template <typename T, typename Tuple>
    struct tuple_index;

    // Специализация для случаев, когда первый в std::tuple соответствует T.
    template <typename T, typename... Rest>
    struct tuple_index<T, std::tuple<T, Rest...>> {
        static constexpr size_t value = 0;
    };

    // Специализация для случаев, когда первый в std::tuple не соответствует T.
    // Рекурсия до прошлой специализации.
    template <typename T, typename U, typename... Rest>
    struct tuple_index<T, std::tuple<U, Rest...>> {
        static constexpr size_t value = 1 + tuple_index<T, std::tuple<Rest...>>::value;
    };

    // При отсутствии типа в std::tuple
    template <typename T>
    struct tuple_index<T, std::tuple<>> {
        static_assert(sizeof(T) == 0, "Type not found in tuple");
    };
#pragma endregion

}