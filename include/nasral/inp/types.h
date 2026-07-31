#pragma once

#include <array>
#include <bitset>
#include <string>
#include <variant>
#include <memory>

namespace nasral::inp
{
    constexpr uint32_t kMaxKeyBindings = 2;

    enum class KeyCode : uint32_t
    {
        eNone = 0,

        // Алфавитно-цифровые клавиши
        eA, eB, eC, eD, eE, eF, eG, eH, eI, eJ, eK, eL, eM,
        eN, eO, eP, eQ, eR, eS, eT, eU, eV, eW, eX, eY, eZ,
        e0, e1, e2, e3, e4, e5, e6, e7, e8, e9,

        // Функциональные клавиши
        eF1, eF2, eF3, eF4, eF5, eF6, eF7, eF8, eF9, eF10, eF11, eF12,

        // Управляющие клавиши
        eEscape,
        eEnter,
        eTab,
        eBackspace,
        eInsert,
        eDelete,
        eRight,
        eLeft,
        eDown,
        eUp,
        ePageUp,
        ePageDown,
        eHome,
        eEnd,
        eCapsLock,
        eScrollLock,
        eNumLock,
        ePrintScreen,
        ePause,

        // Модификаторы
        eLeftShift,
        eRightShift,
        eLeftControl,
        eRightControl,
        eLeftAlt,
        eRightAlt,
        eLeftSuper,
        eRightSuper,
        eMenu,

        // Спецсимволы
        eSpace,
        eApostrophe,    // '
        eComma,         // ,
        eMinus,         // -
        ePeriod,        // .
        eSlash,         // /
        eSemicolon,     // ;
        eEqual,         // =
        eLeftBracket,   // [
        eBackslash,     //
        eRightBracket,  // ]
        eGraveAccent,   // `

        // Клавиши цифровой клавиатуры (Numpad)
        eKp0, eKp1, eKp2, eKp3, eKp4, eKp5, eKp6, eKp7, eKp8, eKp9,
        eKpDecimal,
        eKpDivide,
        eKpMultiply,
        eKpSubtract,
        eKpAdd,
        eKpEnter,
        eKpEqual,

        TOTAL
    };

    enum class MouseButton : uint32_t
    {
        eNone = 0,

        eLeft,
        eRight,
        eMiddle,

        TOTAL
    };

    // typedef std::bitset<static_cast<size_t>(KeyCode::TOTAL)> KeyStateFlags;
    // typedef std::bitset<static_cast<size_t>(MouseButton::TOTAL)> MouseStateFlags;
    typedef std::variant<KeyCode, MouseButton> KeyBinding;

    struct Action
    {
        std::string name;
        std::array<KeyBinding, kMaxKeyBindings> bindings = {KeyCode::eNone};
        size_t binding_count = 0;
    };

    struct ActionDesc
    {
        std::string name;
        std::vector<KeyBinding> bindings;
    };

    class InputProvider;
    struct Config
    {
        std::shared_ptr<InputProvider> provider = nullptr;
        float default_sensitivity = 1.0f;
    };
}
