#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <nlohmann/json.hpp>
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_containers.hpp>
#include <nasral/common/types.h>
#include <nasral/ecs/entity.h>
#include <nasral/gfx/types.h>

// ============================================================================
// 1. Универсальный сериализатор/десериализатор для всех enum-типов (magic_enum)
// ============================================================================
template <typename T>
struct nlohmann::adl_serializer<T, std::enable_if_t<std::is_enum_v<T>>>
{
    static void to_json(json& j, const T& e)
    {
        j = magic_enum::enum_name(e);
    }

    static void from_json(const json& j, T& e)
    {
        if (j.is_string())
        {
            const auto str = j.get<std::string_view>();
            if (auto opt = magic_enum::enum_cast<T>(str); opt.has_value())
            {
                e = opt.value();
            }
        }
        else if (j.is_number_integer())
        {
            if (auto opt = magic_enum::enum_cast<T>(j.get<std::underlying_type_t<T>>()); opt.has_value())
            {
                e = opt.value();
            }
        }
    }
};

// ============================================================================
// 2. nasral::UniqueId
// ============================================================================
namespace nasral
{
    inline void to_json(nlohmann::json& j, const UniqueId& id)
    {
        j = nlohmann::json::array({ id.data[0], id.data[1] });
    }

    inline void from_json(const nlohmann::json& j, UniqueId& id)
    {
        if (j.is_array() && j.size() >= 2)
        {
            id.set(j[0].get<uint64_t>(), j[1].get<uint64_t>());
        }
        else if (j.is_string())
        {
            const auto s = j.get<std::string>();
            const auto sep = s.find_first_of("_. ");
            if (sep != std::string::npos)
            {
                const auto a = std::stoull(s.substr(0, sep));
                const auto b = std::stoull(s.substr(sep + 1));
                id.set(a, b);
            }
            else if (!s.empty())
            {
                id.set(std::stoull(s), 0);
            }
        }
    }
}

// ============================================================================
// 3. nasral::ecs::EntityId и EntityIds<N>
// ============================================================================
namespace nasral::ecs
{
    inline void to_json(nlohmann::json& j, const EntityId& id)
    {
        j = nlohmann::json::array({ id.index, id.version });
    }

    inline void from_json(const nlohmann::json& j, EntityId& id)
    {
        if (j.is_array() && j.size() >= 2)
        {
            id.index = j[0].get<size_t>();
            id.version = j[1].get<size_t>();
        }
        else if (j.is_object())
        {
            id.index = j.value("index", id.index);
            id.version = j.value("version", id.version);
        }
        else if (j.is_number_integer())
        {
            id.index = j.get<size_t>();
            id.version = 0;
        }
        else if (j.is_string())
        {
            auto s = j.get<std::string>();
            if (!s.empty() && s.front() == '[') s.erase(0, 1);
            if (!s.empty() && s.back() == ']') s.pop_back();

            const auto sep = s.find_first_of("._");
            if (sep != std::string::npos)
            {
                id.index = std::stoull(s.substr(0, sep));
                id.version = std::stoull(s.substr(sep + 1));
            }
            else if (!s.empty())
            {
                id.index = std::stoull(s);
                id.version = 0;
            }
        }
    }

    template<std::size_t N>
    void to_json(nlohmann::json& j, const EntityIds<N>& ids)
    {
        j = nlohmann::json::array();
        for (const auto& id : ids)
        {
            j.push_back(id);
        }
    }

    template<std::size_t N>
    void from_json(const nlohmann::json& j, EntityIds<N>& ids)
    {
        ids.clear();
        if (j.is_array())
        {
            for (const auto& item : j)
            {
                ids.push_back(item.get<EntityId>());
            }
        }
    }
}

// ============================================================================
// 4. Специфичные алиасы для Enum (например, eColored -> MaterialType::eColor)
// ============================================================================
namespace nasral::gfx
{
    inline void from_json(const nlohmann::json& j, MaterialType& e)
    {
        if (j.is_string())
        {
            const auto str = j.get<std::string_view>();
            if (const auto opt = magic_enum::enum_cast<MaterialType>(str); opt.has_value())
            {
                e = opt.value();
            }
            else if (str == "eColored")
            {
                e = MaterialType::eColor;
            }
        }
        else if (j.is_number_integer())
        {
            if (const auto opt = magic_enum::enum_cast<MaterialType>(j.get<std::underlying_type_t<MaterialType>>()); opt.has_value())
            {
                e = opt.value();
            }
        }
    }
}

// ============================================================================
// 5. magic_enum::containers::array (EnumArray<E, V>)
// ============================================================================
namespace magic_enum::containers
{
    template <typename E, typename V>
    void to_json(nlohmann::json& j, const array<E, V>& arr)
    {
        j = nlohmann::json::object();
        for (const auto& [val, name] : magic_enum::enum_entries<E>())
        {
            j[std::string(name)] = arr[val];
        }
    }

    template <typename E, typename V>
    void from_json(const nlohmann::json& j, array<E, V>& arr)
    {
        if (!j.is_object()) return;
        for (const auto& [val, name] : magic_enum::enum_entries<E>())
        {
            if (std::string key(name); j.contains(key))
            {
                arr[val] = j.at(key).get<V>();
            }
        }
    }
}