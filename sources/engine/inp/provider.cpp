#include "pch.h"
#include <nasral/inp/provider.h>
#include <nasral/inp/manager.h>

namespace nasral::inp
{
    InputProvider::InputProvider()
        : mouse_pos_({0.0f, 0.0f})
        , keyboard_states_({})
        , mouse_states_({})
    {}

    bool InputProvider::is_key_pressed(const KeyCode key) const
    {
        const auto bit = static_cast<size_t>(key);
        return keyboard_states_.test(bit);
    }

    bool InputProvider::is_mouse_button_pressed(const MouseButton button) const
    {
        const auto bit = static_cast<size_t>(button);
        return mouse_states_.test(bit);
    }

    glm::vec2 InputProvider::mouse_position() const
    {
        return mouse_pos_;
    }

    void InputProvider::on_key_state_changed(const KeyCode code, const bool state)
    {
        const auto bit = static_cast<size_t>(code);
        keyboard_states_.set(bit, state);
    }

    void InputProvider::on_mouse_btn_state_changed(const MouseButton code, const bool state)
    {
        const auto bit = static_cast<size_t>(code);
        mouse_states_.set(bit, state);
    }

    void InputProvider::om_mouse_pos_changed(const float x, const float y)
    {
        mouse_pos_.x = x;
        mouse_pos_.y = y;
    }
}
