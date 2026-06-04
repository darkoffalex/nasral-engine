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

        log_info("Manager initialized");
    }

    void Manager::on_update([[maybe_unused]] float delta){

        if (state_.all_of(StateFlags::eResourcesReady, StateFlags::eMaterialsReady) && !state_.test(StateFlags::eRunning))
        {
            state_.set(StateFlags::eRunning);
            // TODO: Инициировать загрузку сцены
        }
    }

    void Manager::on_finalize(){
        evl_mat_reg_.reset();
        evl_res_reg_.reset();
        log_info("Manager finalized");
    }

    void Manager::on_res_registry_changed(const evt::Arg& arg){
        const auto reason = evt::from_arg<evt::ChangeReason>(arg);
        if (reason == evt::ChangeReason::eInitial){
            state_.set(StateFlags::eResourcesReady);
            log_info("Resource registry initialized");
        }
    }

    void Manager::on_mat_registry_changed(const evt::Arg& arg){
        const auto reason = evt::from_arg<evt::ChangeReason>(arg);
        if (reason == evt::ChangeReason::eInitial){
            state_.set(StateFlags::eMaterialsReady);
            log_info("Material registry initialized");
        }
    }
}
