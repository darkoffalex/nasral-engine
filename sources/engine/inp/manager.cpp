#include "pch.h"
#include <nasral/inp/manager.h>
#include <nasral/inp/provider.h>
#include <nasral/evt/utils.h>
#include <nasral/res/objects/project.h>
#include <nasral/engine.h>

namespace nasral::inp
{
    Manager::Manager(Engine* e, const Config& config)
        : Subsystem(e, config)
        , prev_mouse_pos_({0.0f, 0.0f})
        , prev_keyboard_states_({})
        , prev_mouse_states_({})
        , sensitivity_(config.default_sensitivity)
    {
        if (!config.provider){
            throw std::runtime_error("Input provider is not set");
        }
    }

    Manager::~Manager()
    = default;

    void Manager::on_init()
    {
        // Слушать событие загрузки проекта
        evl_on_proj_load_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eProjectFileLoaded,
            evt::bind(this, &Manager::on_project_loaded));

        log_info("Manager initialized");
    }

    void Manager::on_update([[maybe_unused]] float delta)
    {
        prev_mouse_pos_ = mouse_position();
        prev_keyboard_states_ = config().provider->keyboard_states();
        prev_mouse_states_ = config().provider->mouse_states();

        if (config().provider->consume_surface_resized())
        {
            engine()->events()->send(
                evt::Type::eDisplaySurfaceChanged,
                evt::ChangeReason::eResized);
        }
    }

    void Manager::on_finalize()
    {
        evl_on_proj_load_.reset();
        log_info("Manager finalized");
    }

    bool Manager::is_key_pressed(const KeyCode code) const
    {
        return config().provider->is_key_pressed(code);
    }

    bool Manager::is_key_just_pressed(const KeyCode code) const
    {
        return is_key_pressed(code) && !prev_keyboard_states_.test(code);
    }

    bool Manager::is_mouse_btn_pressed(const MouseButton button) const
    {
        return config().provider->is_mouse_btn_pressed(button);
    }

    bool Manager::is_mouse_btn_just_pressed(const MouseButton button) const
    {
        return is_mouse_btn_pressed(button) && !prev_mouse_states_.test(button);
    }

    glm::vec2 Manager::mouse_position() const
    {
        return config().provider->mouse_position();
    }

    glm::vec2 Manager::mouse_delta(const bool use_sensitivity) const
    {
        return (mouse_position() - prev_mouse_pos_) * (use_sensitivity ? sensitivity_ : 1.0f);
    }

    std::optional<size_t> Manager::action_index(const std::string& name) const
    {
        const auto it = action_indices_.find(name);
        if (it == action_indices_.end()) return std::nullopt;
        return it->second;
    }

    bool Manager::is_action_pressed(const size_t index) const
    {
        if (index >= actions_.size()) return false;
        const auto& bindings = actions_[index].bindings;
        for (const auto& binding : bindings){
            if (std::holds_alternative<KeyCode>(binding)){
                return is_key_pressed(std::get<KeyCode>(binding));
            }
            if (std::holds_alternative<MouseButton>(binding)){
                return is_mouse_btn_pressed(std::get<MouseButton>(binding));
            }
        }
        return false;
    }

    bool Manager::is_action_just_pressed(const size_t index) const
    {
        if (index >= actions_.size()) return false;
        const auto& bindings = actions_[index].bindings;
        for (const auto& binding : bindings){
            if (std::holds_alternative<KeyCode>(binding)){
                return is_key_just_pressed(std::get<KeyCode>(binding));
            }
            if (std::holds_alternative<MouseButton>(binding)){
                return is_mouse_btn_just_pressed(std::get<MouseButton>(binding));
            }
        }
        return false;
    }

    glm::vec3 Manager::get_movement_vector(const KeyCode left_key
        , const KeyCode right_key
        , const KeyCode forward_key
        , const KeyCode backward_key
        , const KeyCode up_key
        , const KeyCode down_key
        , const bool normalize) const
    {
        auto result = glm::vec3(0.0f);

        if (is_key_pressed(left_key)){
            result.x += -1.0f;
        }else if (is_key_pressed(right_key)){
            result.x += 1.0f;
        }

        if (is_key_pressed(forward_key)){
            result.z += -1.0f;
        }else if (is_key_pressed(backward_key)){
            result.z += 1.0f;
        }

        if (is_key_pressed(up_key)){
            result.y += 1.0f;
        }else if (is_key_pressed(down_key)){
            result.y += -1.0f;
        }

        if (glm::length2(result) == 0.0f){
            return glm::vec3(0.0f);
        }

        return normalize ? glm::normalize(result) : result;
    }

    glm::vec3 Manager::get_movement_vector(const std::string& left_act
        , const std::string& right_act
        , const std::string& forward_act
        , const std::string& backward_act
        , const std::string& up_act
        , const std::string& down_act
        , const bool normalize) const
    {
        auto result = glm::vec3(0.0f);

        const auto left_act_idx     = action_index(left_act);
        const auto right_act_idx    = action_index(right_act);
        const auto forward_act_idx  = action_index(forward_act);
        const auto backward_act_idx = action_index(backward_act);
        const auto up_act_idx       = action_index(up_act);
        const auto down_act_idx     = action_index(down_act);


        if (left_act_idx.has_value() && is_action_pressed(left_act_idx.value())){
            result.x += -1.0f;
        } else if (right_act_idx.has_value() && is_action_pressed(right_act_idx.value())){
            result.x += 1.0f;
        }

        if (forward_act_idx.has_value() && is_action_pressed(forward_act_idx.value())){
            result.z += -1.0f;
        } else if (backward_act_idx.has_value() && is_action_pressed(backward_act_idx.value())){
            result.z += 1.0f;
        }

        if (up_act_idx.has_value() && is_action_pressed(up_act_idx.value())){
            result.y += 1.0f;
        } else if (down_act_idx.has_value() && is_action_pressed(down_act_idx.value())){
            result.y += -1.0f;
        }

        return normalize ? glm::normalize(result) : result;
    }

    void Manager::register_action(const ActionDesc& action_desc)
    {
        actions_.push_back({action_desc.name, {}});
        auto& [name, bindings, bindings_count] = actions_.back();

        for (size_t i = 0; i < action_desc.bindings.size(); ++i){
            if (i >= bindings.size()) break;
            bindings[i] = action_desc.bindings[i];
            bindings_count++;
        }

        action_indices_[name] = static_cast<uint32_t>(actions_.size() - 1);
    }

    void Manager::reset_actions()
    {
        actions_.clear();
        action_indices_.clear();
    }

    void Manager::on_project_loaded(const evt::Arg& arg)
    {
        // Получить ресурс файла проекта
        auto* res = evt::from_arg<res::Resource*>(arg).value_or(nullptr);
        const auto* proj = dynamic_cast<res::ProjectFile*>(res);

        assert(res && "Wrong project file resource");
        assert(res->status() == res::Status::eLoaded && "Project file resource is not loaded");
        assert(proj && "Project file resource is not a project file");

        // Зарегистрировать действие
        for (const auto& ab : proj->action_bindings()){
            register_action(ab);
        }

        // Настройки ввода готовы
        engine()->events()->send_deferred(
            evt::Type::eInputSettingsChanged,
            evt::ChangeReason::eInitial);
    }
}
