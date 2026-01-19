#pragma once

#include <memory>
#include <glm/vec2.hpp>
#include <nasral/inp/types.h>

namespace nasral::inp
{
    class InputProvider
    {
    public:
        typedef std::shared_ptr<InputProvider> Ptr;

        InputProvider();
        virtual ~InputProvider() = default;

        [[nodiscard]] bool is_key_pressed(KeyCode key) const;
        [[nodiscard]] bool is_mouse_button_pressed(MouseButton button) const;
        [[nodiscard]] glm::vec2 mouse_position() const;

        [[nodiscard]] const KeyStateFlags& keyboard_states() const noexcept { return keyboard_states_; }
        [[nodiscard]] const MouseStateFlags& mouse_states() const noexcept { return mouse_states_; }

    protected:
        void on_key_state_changed(KeyCode code, bool state);
        void on_mouse_btn_state_changed(MouseButton code, bool state);
        void om_mouse_pos_changed(float x, float y);

    private:
        glm::vec2 mouse_pos_;
        KeyStateFlags keyboard_states_;
        MouseStateFlags mouse_states_;
    };
}
