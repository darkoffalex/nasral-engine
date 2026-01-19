#include "pch.h"
#include <nasral/inp/manager.h>
#include <nasral/inp/provider.h>
#include <nasral/engine.h>
#include <nasral/evt/utils.h>
#include <nasral/res/resources/project.h>

namespace nasral::inp
{
    Manager::Manager(Engine* engine, const Config& config)
        : Subsystem(engine, config)
        , evt_h_proj_load_(evt::kInvalidListener)
        , prev_mouse_pos_({0.0f, 0.0f})
        , prev_keyboard_states_({})
        , prev_mouse_states_({})
        , sensitivity_(config.default_sensitivity)
    {
        if (!config.provider){
            throw std::runtime_error("No input provider specified");
        }

        evt_h_proj_load_ = engine->events()->register_l(
            evt::Type::eProjectResLoaded,
            evt::bind(this, &Manager::on_project_loaded));
    }

    Manager::~Manager()
    {
        engine()->events()->unregister_l(
            evt::Type::eProjectResLoaded,
            evt_h_proj_load_);
    }

    void Manager::update([[maybe_unused]] float dt)
    {
        prev_mouse_pos_ = mouse_position();
        prev_keyboard_states_ = config().provider->keyboard_states();
        prev_mouse_states_ = config().provider->mouse_states();
    }

    bool Manager::is_key_pressed(const KeyCode code) const
    {
        return config().provider->is_key_pressed(code);
    }

    bool Manager::is_key_just_pressed(const KeyCode code) const
    {
        const auto bit = static_cast<size_t>(code);
        return is_key_pressed(code) && !prev_keyboard_states_.test(bit);
    }

    bool Manager::is_mouse_btn_pressed(const MouseButton button) const
    {
        return config().provider->is_mouse_button_pressed(button);
    }

    bool Manager::is_mouse_btn_just_pressed(const MouseButton button) const
    {
        const auto bit = static_cast<size_t>(button);
        return is_mouse_btn_pressed(button) && !prev_mouse_states_.test(bit);
    }

    glm::vec2 Manager::mouse_position() const
    {
        return config().provider->mouse_position();
    }

    glm::vec2 Manager::mouse_delta(const bool use_sensitivity) const
    {
        return (mouse_position() - prev_mouse_pos_) * (use_sensitivity ? sensitivity_ : 1.0f);
    }

    bool Manager::is_action_pressed(const std::string& name) const
    {
        const auto it = action_indices_.find(name);
        if (it == action_indices_.end()) return false;
        return is_action_pressed(it->second);
    }

    bool Manager::is_action_just_pressed(const std::string& name) const
    {
        const auto it = action_indices_.find(name);
        if (it == action_indices_.end()) return false;
        return is_action_just_pressed(it->second);
    }

    bool Manager::is_action_pressed(const size_t index) const
    {
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

    glm::vec3 Manager::get_mouse_movement_vector(const std::string& left_act
        , const std::string& right_act
        , const std::string& forward_act
        , const std::string& backward_act
        , const std::string& up_act
        , const std::string& down_act
        , const bool normalize) const
    {
        auto result = glm::vec3(0.0f);

        if (is_action_pressed(left_act)){
            result.x += -1.0f;
        }else if (is_action_pressed(right_act)){
            result.x += 1.0f;
        }

        if (is_action_pressed(forward_act)){
            result.z += -1.0f;
        }else if (is_action_pressed(backward_act)){
            result.z += 1.0f;
        }

        if (is_action_pressed(up_act)){
            result.y += 1.0f;
        }else if (is_action_pressed(down_act)){
            result.y += -1.0f;
        }

        if (glm::length2(result) == 0.0f){
            return glm::vec3(0.0f);
        }

        return normalize ? glm::normalize(result) : result;
    }

    void Manager::register_action(const std::string& name, const std::vector<ActionBinding>& bindings)
    {
        actions_.push_back({name, {}});
        auto& [added_name, added_bindings] = actions_.back();

        for (size_t i = 0; i < bindings.size(); ++i){
            if (i >= added_bindings.size()) break;
            added_bindings[i] = bindings[i];
        }

        action_indices_[added_name] = static_cast<uint32_t>(actions_.size() - 1);
    }

    void Manager::reset_actions()
    {
        actions_.clear();
        action_indices_.clear();
    }

    void Manager::on_project_loaded(const evt::Arg& arg)
    {
        auto* r_ptr = evt::from_arg<res::IResource*>(arg).value_or(nullptr);
        if (const auto* proj = dynamic_cast<res::Project*>(r_ptr))
        {
            if (!proj->action_bindings().empty()){
                reset_actions();
                for (const auto& [name, bindings] : proj->action_bindings()){
                    register_action(name, bindings);
                }
            }

            if (proj->sensitivity() > 0.0f){
                sensitivity_ = proj->sensitivity();
            }
        }
    }
}
