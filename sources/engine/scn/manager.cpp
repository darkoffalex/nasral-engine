#include "pch.h"
#include <nasral/scn/manager.h>
#include <nasral/evt/utils.h>
#include <nasral/res/resources/project.h>
#include <nasral/engine.h>

namespace nasral::scn
{
    Manager::Manager(Engine* e, const Config& cfg)
        : Subsystem(e, cfg)
        , root_({})
    {
        evt_h_proj_load_ = engine()->events()->register_l(
            evt::Type::eProjectResLoaded,
            evt::bind(this, &Manager::on_project_loaded));
    }

    Manager::~Manager()
    {
        engine()->events()->unregister_l(evt::Type::eProjectResLoaded, evt_h_proj_load_);
    }

    void Manager::on_project_loaded(const evt::Arg& arg) const
    {
        auto* r_ptr = static_cast<res::IResource*>(*std::get_if<evt::ArgPtr>(&arg));
        if (const auto* proj = dynamic_cast<res::Project*>(r_ptr)){
            assert(proj->status() == res::Status::eLoaded);

            // TODO: Добавить нужный компонент к сцене
        }
    }
}
