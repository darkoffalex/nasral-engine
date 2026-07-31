#include "pch.h"
#include <nasral/run/manager.h>
#include <nasral/evt/utils.h>
#include <nasral/engine.h>

namespace nasral::run
{
    Manager::Manager(Engine* e, const Config& config)
        : Subsystem(e, config)
    {
        log_info("Initializing manager...");
    }

    Manager::~Manager(){
        log_info("Manager destroyed");
    }

    void Manager::on_init()
    {
        evl_mat_reg_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eMaterialRegistryChanged,
            evt::bind(this, &Manager::on_mat_registry_changed));

        evl_res_reg_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eResourceRegistryChanged,
            evt::bind(this, &Manager::on_res_registry_changed));

        evl_bindings_ = evt::Listener::reg(
            engine()->events(),
            evt::Type::eInputSettingsChanged,
            evt::bind(this, &Manager::on_key_bindings_changed));

        log_info("Manager initialized");
    }

    void Manager::on_update([[maybe_unused]] float delta)
    {
        // Если регистр ресурсов и материалов сформирован, и если сеанс еще не начат
        if (state_.has(StateFlags::eResourcesReady, StateFlags::eMaterialsReady, StateFlags::eKeuBindingsReady) &&
            state_.has_no(StateFlags::eRunning))
        {
            // Смена состояния (запущено)
            state_.set(StateFlags::eRunning);
            log_info("Session started");
            // Событие начала сеанса
            engine()->events()->send(evt::Type::eSessionStarted, evt::kNullArg);
        }
    }

    void Manager::on_finalize(){
        evl_mat_reg_.reset();
        evl_res_reg_.reset();
        evl_bindings_.reset();
        log_info("Manager finalized");
    }

    void Manager::on_res_registry_changed(const evt::Arg& arg){
        switch (evt::from_arg<evt::ChangeReason>(arg).value_or(evt::ChangeReason::eInitial))
        {
        case evt::ChangeReason::eInitial:
            {
                state_.set(StateFlags::eResourcesReady);
                log_info("Resource registry initialized");
                break;
            }
        default:{}
        }
    }

    void Manager::on_mat_registry_changed(const evt::Arg& arg){
        switch (evt::from_arg<evt::ChangeReason>(arg).value_or(evt::ChangeReason::eInitial))
        {
        case evt::ChangeReason::eInitial:
            {
                state_.set(StateFlags::eMaterialsReady);
                log_info("Material registry initialized");
            }
        default:{}
        }
    }

    void Manager::on_key_bindings_changed(const evt::Arg& arg)
    {
        switch (evt::from_arg<evt::ChangeReason>(arg).value_or(evt::ChangeReason::eInitial))
        {
        case evt::ChangeReason::eInitial:
            {
                state_.set(StateFlags::eKeuBindingsReady);
                log_info("Key bindings initialized");
            }
        default:{}
        }
    }
}
