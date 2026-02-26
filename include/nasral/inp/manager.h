#pragma once

#include <glm/glm.hpp>
#include <nasral/core/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/inp/types.h>
#include <nasral/evt/types.h>

namespace nasral::inp
{
    class Manager final : public core::Subsystem<Config>, public log::Loggable<Manager>
    {
    public:
        using Ptr = std::unique_ptr<Manager>;

        Manager(Engine* engine, const Config& config);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void update(float dt);
        [[nodiscard]] bool is_key_pressed(KeyCode code) const;
        [[nodiscard]] bool is_key_just_pressed(KeyCode code) const;
        [[nodiscard]] bool is_mouse_btn_pressed(MouseButton button) const;
        [[nodiscard]] bool is_mouse_btn_just_pressed(MouseButton button) const;
        [[nodiscard]] glm::vec2 mouse_position() const;
        [[nodiscard]] glm::vec2 mouse_delta(bool use_sensitivity = true) const;

        [[nodiscard]] bool is_action_pressed(const std::string& name) const;
        [[nodiscard]] bool is_action_just_pressed(const std::string& name) const;
        [[nodiscard]] bool is_action_pressed(size_t index) const;
        [[nodiscard]] bool is_action_just_pressed(size_t index) const;

        [[nodiscard]] glm::vec3 get_movement_vector(KeyCode left_key
            , KeyCode right_key
            , KeyCode forward_key
            , KeyCode backward_key
            , KeyCode up_key
            , KeyCode down_key
            , bool normalize = true) const;

        [[nodiscard]] glm::vec3 get_mouse_movement_vector(const std::string& left_act
            , const std::string& right_act
            , const std::string& forward_act
            , const std::string& backward_act
            , const std::string& up_act
            , const std::string& down_act
            , bool normalize = true) const;

    private:
        void register_action(const std::string& name, const std::vector<ActionBinding>& bindings = {});
        void reset_actions();
        void on_project_loaded(const evt::Arg& arg);

    protected:
        evt::ListenerHandle     evt_h_proj_load_;
        glm::vec2               prev_mouse_pos_;
        KeyStateFlags           prev_keyboard_states_;
        MouseStateFlags         prev_mouse_states_;
        float                   sensitivity_;

        std::vector<Action>     actions_;
        std::unordered_map<std::string_view, size_t> action_indices_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(nasral::inp::Manager)
