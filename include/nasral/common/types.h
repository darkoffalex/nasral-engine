#pragma once

#include <random>
#include <bitset>
#include <magic_enum/magic_enum_containers.hpp>

namespace nasral
{
#ifndef NDEBUG
    constexpr bool kDebugBuild = true;
#else
    constexpr bool kDebugBuild = false;
#endif

    template <typename E, typename V>
    using EnumArray = magic_enum::containers::array<E, V>;

    template <typename E>
    struct EnumMask
    {
        static_assert(std::is_enum_v<E>, "EnumMask can only be used with enum types.");
        std::bitset<magic_enum::enum_count<E>()> bitset;

        void set(E e) {
            bitset.set(static_cast<std::size_t>(magic_enum::enum_integer(e)));
        }

        void reset(E e) {
            bitset.reset(static_cast<std::size_t>(magic_enum::enum_integer(e)));
        }

        [[nodiscard]] bool test(E e) const {
            return bitset.test(static_cast<std::size_t>(magic_enum::enum_integer(e)));
        }

        [[nodiscard]] bool any() const {
            return bitset.any();
        }

        [[nodiscard]] bool all() const {
            return bitset.all();
        }

        template <typename... Args>
        [[nodiscard]] constexpr bool any_of(const Args... args) const {
            return (test(static_cast<E>(args)) || ...);
        }

        template <typename... Args>
        [[nodiscard]] constexpr bool all_of(const Args... args) const {
            return (test(static_cast<E>(args)) && ...);
        }

        template <typename... Args>
        [[nodiscard]] constexpr bool none_of(const Args... args) const {
            return !(test(static_cast<E>(args)) || ...);
        }

        void clear() {
            bitset.reset();
        }
    };

    /**
     * @brief Уникальный ID
     * @details Используется для сохранения/записи связей (ссылок) между различными сущностями.
     */
    struct UniqueId
    {
        uint64_t data[2] = {0, 0};

        UniqueId() = default;
        UniqueId(const uint64_t a, const uint64_t b) { set(a, b); }

        bool operator==(const UniqueId& other) const noexcept {
            return data[0] == other.data[0] && data[1] == other.data[1];
        }

        bool operator!=(const UniqueId& other) const noexcept{
            return !(*this == other);
        }

        bool operator<(const UniqueId& other) const noexcept {
            if (data[0] < other.data[0]) return true;
            if (data[0] > other.data[0]) return false;
            return data[1] < other.data[1];
        }

        void regenerate(){
            const auto new_id = generate();
            data[0] = new_id.data[0];
            data[1] = new_id.data[1];
        }

        static UniqueId generate(){
            static std::mt19937_64 rng{std::random_device{}()};
            static std::uniform_int_distribution<uint64_t> dist;
            return {dist(rng), dist(rng)};
        }

        void set(const uint64_t a, const uint64_t b){
            data[0] = a;
            data[1] = b;
        }

        [[nodiscard]] std::string to_string() const{
            return std::to_string(data[0]) + "_" + std::to_string(data[1]);
        }
    };

    /**
     * @brief Хеширование для уникального ID
     * @details Необходимо для использования UniqueID как ключа к st::unordered_map
     */
    struct UniqueIdHash
    {
        size_t operator()(const UniqueId& id) const noexcept
        {
            const size_t h1 = std::hash<uint64_t>{}(id.data[0]);
            const size_t h2 = std::hash<uint64_t>{}(id.data[1]);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };
}

/**
 * @brief Специализация шаблона std::hash для nasral::core::UniqueId
 * Позволит не передавать UniqueIdHash при использовании std::unordered_map<UniqueId>.
 */
template<>
struct std::hash<nasral::UniqueId>
{
    size_t operator()(const nasral::UniqueId& id) const noexcept
    {
        return nasral::UniqueIdHash{}(id);
    }
};
