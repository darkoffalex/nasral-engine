#include "pch.h"
#include <nasral/scn/manager.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Manager::Manager(Engine* e, const Config& config): Subsystem(e, config){
        log_info("Initializing manager...");
    }

    Manager::~Manager(){
        log_info("Manager destroyed");
    }

    void Manager::on_init() const{
        log_info("Manager initialized");
    }

    void Manager::on_update([[maybe_unused]] float delta){

    }

    void Manager::on_finalize() const{
        log_info("Manager finalized");
    }
}
