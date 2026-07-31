#pragma once

#include <memory>
#include <glm/vec2.hpp>
#include <nasral/common/types.h>
#include <nasral/inp/types.h>

namespace nasral::inp
{
    class InputProvider
    {
    public:
        typedef std::shared_ptr<InputProvider> Ptr;

        InputProvider();
        virtual ~InputProvider();

        [[nodiscard]] bool is_key_pressed(KeyCode key) const;
        [[nodiscard]] bool is_mouse_btn_pressed(MouseButton button) const;
        [[nodiscard]] bool is_window_resized() const;
        [[nodiscard]] bool consume_surface_resized();
        [[nodiscard]] const glm::vec2& mouse_position() const;

        [[nodiscard]] const auto& keyboard_states() const noexcept { return keyboard_states_; }
        [[nodiscard]] const auto& mouse_states() const noexcept { return mouse_states_; }

    protected:
        void on_key_state_changed(KeyCode code, bool state);
        void on_mouse_btn_state_changed(MouseButton code, bool state);
        void om_mouse_pos_changed(float x, float y);
        void on_window_surface_resized(int width, int height);

    private:
        glm::vec2 mouse_pos_;
        EnumMask<KeyCode> keyboard_states_;
        EnumMask<MouseButton> mouse_states_;
        std::atomic<bool> surface_resized_;
    };
}
