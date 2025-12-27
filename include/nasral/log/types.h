#pragma once
#include <string>
#include <string_view>
#include <cstdint>

namespace nasral::log
{
    enum class Level : uint32_t
    {
        eNone       = 0,
        eDebug      = 1 << 0,
        eInfo       = 1 << 1,
        eWarning    = 1 << 2,
        eError      = 1 << 3,
        eFatal      = 1 << 4,
        eAll        = eDebug | eInfo | eWarning | eError | eFatal
    };

    constexpr std::array<std::string_view, 5> kLevelNames = {
        "[DEBUG]",
        "[INFO]",
        "[WARNING]",
        "[ERROR]",
        "[FATAL]"
    };

    constexpr Level operator|(Level a, Level b) {
        return static_cast<Level>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    constexpr Level operator&(Level a, Level b) {
        return static_cast<Level>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    constexpr Level operator~(Level a) {
        return static_cast<Level>(~static_cast<uint32_t>(a));
    }

    constexpr auto kLevelDev        = Level::eAll;
    constexpr auto kLevelProd       = Level::eError | Level::eFatal;
    constexpr auto kLevelVerbose    = Level::eInfo | Level::eWarning | Level::eError;

    struct Config
    {
        Level level = kLevelDev;
        bool console = false;
        std::string file;
    };
}
