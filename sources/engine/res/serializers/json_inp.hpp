#pragma once

#include <nlohmann/json.hpp>
#include <nasral/inp/types.h>

#include "./json_common.hpp"
#include "./json_glm.hpp"

namespace nasral::inp
{
    inline void from_json(const nlohmann::json& j, ActionDesc& desc)
    {
        desc.name = j.value("action", j.value("name", desc.name));
        desc.bindings.clear();

        if (j.contains("keys") && j.at("keys").is_array())
        {
            for (const auto& key_entry_val : j.at("keys"))
            {
                if (!key_entry_val.is_string()) continue;
                const auto key_entry = key_entry_val.get<std::string>();

                if (key_entry.rfind("key:", 0) == 0)
                {
                    const auto key_str = key_entry.substr(4);
                    if (auto key_e = magic_enum::enum_cast<KeyCode>(key_str); key_e.has_value())
                    {
                        desc.bindings.emplace_back(key_e.value());
                    }
                }
                else if (key_entry.rfind("mouse:", 0) == 0)
                {
                    const auto btn_str = key_entry.substr(6);
                    if (auto btn_e = magic_enum::enum_cast<MouseButton>(btn_str); btn_e.has_value())
                    {
                        desc.bindings.emplace_back(btn_e.value());
                    }
                }
            }
        }
    }

    inline void to_json(nlohmann::json& j, const ActionDesc& desc)
    {
        j = nlohmann::json{
            {"action", desc.name}
        };

        auto keys = nlohmann::json::array();
        for (const auto& binding : desc.bindings)
        {
            std::visit([&keys](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, KeyCode>)
                {
                    keys.push_back(std::string("key:") + std::string(magic_enum::enum_name(val)));
                }
                else if constexpr (std::is_same_v<T, MouseButton>)
                {
                    keys.push_back(std::string("mouse:") + std::string(magic_enum::enum_name(val)));
                }
            }, binding);
        }
        j["keys"] = keys;
    }
}