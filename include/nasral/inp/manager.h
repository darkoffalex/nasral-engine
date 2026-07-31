#pragma once

#include <glm/glm.hpp>
#include <nasral/common/subsystem.h>
#include <nasral/log/loggable.h>
#include <nasral/inp/types.h>
#include <nasral/evt/objects/listener.h>

namespace nasral::inp
{
    class Manager final : public Subsystem<Manager, Config>, public log::Loggable<Manager>
    {
    public:
        typedef std::unique_ptr<Manager> Ptr;

        explicit Manager(Engine* e, const Config& config);
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        void on_init();
        void on_update(float delta);
        void on_finalize();

        [[nodiscard]] bool is_key_pressed(KeyCode code) const;
        [[nodiscard]] bool is_key_just_pressed(KeyCode code) const;
        [[nodiscard]] bool is_mouse_btn_pressed(MouseButton button) const;
        [[nodiscard]] bool is_mouse_btn_just_pressed(MouseButton button) const;
        [[nodiscard]] glm::vec2 mouse_position() const;
        [[nodiscard]] glm::vec2 mouse_delta(bool use_sensitivity = true) const;

        [[nodiscard]] std::optional<size_t> action_index(const std::string& name) const;
        [[nodiscard]] bool is_action_pressed(size_t index) const;
        [[nodiscard]] bool is_action_just_pressed(size_t index) const;

        [[nodiscard]] glm::vec3 get_movement_vector(KeyCode left_key
            , KeyCode right_key
            , KeyCode forward_key
            , KeyCode backward_key
            , KeyCode up_key
            , KeyCode down_key
            , bool normalize = true) const;

        [[nodiscard]] glm::vec3 get_movement_vector(const std::string& left_act
            , const std::string& right_act
            , const std::string& forward_act
            , const std::string& backward_act
            , const std::string& up_act
            , const std::string& down_act
            , bool normalize = true) const;

    protected:
        void on_project_loaded(const evt::Arg& arg);
        void register_action(const ActionDesc& action_desc);
        void reset_actions();

    private:
        /// Слушатель события загрузки проекта
        evt::Listener::Ptr evl_on_proj_load_;
        /// Положение мыши на предыдущем кадре
        glm::vec2 prev_mouse_pos_;
        /// Состояние кнопок клавиатуры на предыдущем кадре
        EnumMask<KeyCode> prev_keyboard_states_;
        // KeyStateFlags prev_keyboard_states_;
        /// Состояние кнопок мыши на предыдущем кадре
        EnumMask<MouseButton> prev_mouse_states_;
        // MouseStateFlags prev_mouse_states_;
        /// Чувствительность мыши
        float sensitivity_;

        /// Зарегистрированные действия (привязка к кнопкам)
        std::vector<Action> actions_;
        std::unordered_map<std::string_view, size_t> action_indices_;
    };
}

DECLARE_SUBSYSTEM_LOGGER_ACCESSOR(inp::Manager, "INP")