#include "pch.h"
#include <nasral/inp/provider.h>

namespace nasral::inp
{
    InputProvider::InputProvider()
        : mouse_pos_({0.0f, 0.0f})
        , keyboard_states_({})
        , mouse_states_({})
        , surface_resized_(false)
    {}

    InputProvider::~InputProvider()
    = default;

    bool InputProvider::is_key_pressed(const KeyCode key) const
    {
        return keyboard_states_.test(key);
    }

    bool InputProvider::is_mouse_btn_pressed(const MouseButton button) const
    {
        return mouse_states_.test(button);
    }

    bool InputProvider::is_window_resized() const
    {
        return surface_resized_.load(std::memory_order_acquire);
    }

    bool InputProvider::consume_surface_resized()
    {
        return surface_resized_.exchange(false, std::memory_order_acquire);
    }

    const glm::vec2& InputProvider::mouse_position() const
    {
        return mouse_pos_;
    }

    void InputProvider::on_key_state_changed(const KeyCode code, const bool state)
    {
        keyboard_states_.set(code, state);
    }

    void InputProvider::on_mouse_btn_state_changed(const MouseButton code, const bool state)
    {
        mouse_states_.set(code, state);
    }

    void InputProvider::om_mouse_pos_changed(const float x, const float y)
    {
        mouse_pos_.x = x;
        mouse_pos_.y = y;
    }

    void InputProvider::on_window_surface_resized([[maybe_unused]] int width, [[maybe_unused]] int height)
    {
        surface_resized_.store(true, std::memory_order_release);
    }
}
