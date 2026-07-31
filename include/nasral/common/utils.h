#pragma once

#include <tuple>
#include <variant>
#include <vector>
#include <magic_enum/magic_enum_containers.hpp>

namespace nasral
{
    /**
     * @brief Конвертация std::tuple<T1, T2..> в std::variant<std::vector<T1>, std::vector<T2>..>
     * @details Общая специализация (объявление)
     * @tparam Tuple Первый параметр - Tuple
     * @tparam IndexSeq Второй параметр - std::index_sequence
     */
    template <typename Tuple, typename IndexSeq>
    struct to_variant_vector_impl;

    /**
     * @brief Конвертация std::tuple<T1, T2..> в std::variant<std::vector<T1>, std::vector<T2>..>
     * @tparam Tuple Первый параметр - Tuple
     * @tparam Is Размер std::index_sequence (берется из переданного std::index_sequence)
     */
    template <typename Tuple, std::size_t... Is>
    struct to_variant_vector_impl<Tuple, std::index_sequence<Is...>> {
        using type = std::variant<std::vector<std::tuple_element_t<Is, Tuple>>...>;
    };

    /**
     * @brief Конвертация std::tuple<T1, T2..> в std::variant<std::vector<T1>, std::vector<T2>..>
     * @details Утилита использующая to_variant_vector_impl
     */
    template <typename Tuple>
    using to_variant_vector = typename to_variant_vector_impl<
        Tuple,
        std::make_index_sequence<std::tuple_size_v<Tuple>>
    >::type;

    /**
     * @brief False для шаблонных выражений
     */
    template <typename...>
    inline constexpr bool always_false_v = false;

    /**
     * @brief Получить индекс типа в std::tuple (рекурсивная compile-time функция)
     * @tparam T Искомый тип
     * @tparam Tuple Tuple
     * @tparam I Изначальный индекс (0 по умолчанию)
     * @return Тип индекса
     */
    template <typename T, typename Tuple, std::size_t I = 0>
    constexpr std::size_t tuple_index(){
        if constexpr (I >= std::tuple_size_v<Tuple>){
            static_assert(always_false_v<T>, "Type not found in tuple");
            return 0;
        }
        else if constexpr (std::is_same_v<std::tuple_element_t<I, Tuple>, T>){
            return I;
        }else{
            return tuple_index<T, Tuple, I + 1>();
        }
    }

    /**
     * @brief Проверить все ли типы std::tuple default constructible
     * @details Общая специализация (объявление)
     * @tparam Tuple Типы Tuple
     */
    template<typename Tuple>
    struct all_default_constructible;

    /**
     * @brief Проверить все ли типы std::tuple default constructible
     * @details Основная специализация (для std::tiple<Ts...>)
     * @tparam Ts Типы в std::tuple
     */
    template<typename... Ts>
    struct all_default_constructible<std::tuple<Ts...>>
        : std::bool_constant<(std::is_default_constructible_v<Ts> && ...)> {};

    /**
     * @brief Утилита для all_default_constructible
     * @tparam Tuple Tuple
     */
    template<typename Tuple>
    inline constexpr bool all_default_constructible_v =
        all_default_constructible<Tuple>::value;

    /**
     * @brief Конверсия enum array в std::vector (копирование)
     * @tparam E Тип ключей
     * @tparam V Тип значений
     * @param arr Массив
     * @param limit Лимит количества
     * @return Вектор
     */
    template <typename E, typename V>
    std::vector<V> to_vec(const magic_enum::containers::array<E, V>& arr, size_t limit = 0) {
        std::vector<V> result;
        auto size = limit > 0 ? std::min(limit, arr.size()) : arr.size();
        result.reserve(size);
        result.insert(result.end(), arr.begin(), arr.begin() + size);
        return result;
    }

    /**
     * @brief Конверсия enum array в std::vector (перемещение)
     * @tparam E Тип ключей
     * @tparam V Тип значений
     * @param arr Массив
     * @param limit Лимит количества
     * @return Вектор
     */
    template <typename E, typename V>
    std::vector<V> to_vec(magic_enum::containers::array<E, V>&& arr, size_t limit = 0) {
        std::vector<V> result;
        auto size = limit > 0 ? std::min(limit, arr.size()) : arr.size();
        result.reserve(size);

        for (size_t i = 0; i < size; ++i){
            result.emplace_back(std::move(arr[i]));
        }
        return result;
    }

    /**
     * @brief Инвертированый обход (std::apply) по типам std::tuple (реализация)
     * @tparam Func Тип функции обратного вызова
     * @tparam Tuple Тип кортежа
     * @tparam Is Индексы
     * @param f Функция обратного вызова
     * @param t Кортеж
     * @return Результат вызова в обратном порядке
     */
    template <typename Func, typename Tuple, std::size_t... Is>
    decltype(auto) apply_reverse_impl(Func&& f, Tuple&& t, std::index_sequence<Is...>) {
        // Вычисляем индексы от (N-1) до 0 и передаем в callable-объект
        return std::invoke(std::forward<Func>(f), std::get<sizeof...(Is) - 1 - Is>(std::forward<Tuple>(t))...);
    }

    /**
     * @brief Инвертированный обход (std::apply) по типам std::tuple
     * @tparam Func Тип функции обратного вызова
     * @tparam Tuple Тип кортежа
     * @param f Функция обратного вызова
     * @param t Кортеж
     * @return Результат вызова в обратном порядке
     */
    template <typename Func, typename Tuple>
    decltype(auto) apply_reverse(Func&& f, Tuple&& t) {
        constexpr auto Size = std::tuple_size_v<std::decay_t<Tuple>>;
        return apply_reverse_impl(std::forward<Func>(f), std::forward<Tuple>(t), std::make_index_sequence<Size>{});
    }
}

#define DECLARE_DETECTOR(method) \
    template <typename T, typename... Args> \
    using has_##method##_t = decltype(std::declval<T>().method(std::declval<Args>()...));
