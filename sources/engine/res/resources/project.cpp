#include "pch.h"
#include <nasral/res/resources/project.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Project::Project(Manager* manager, const ResourceId id, Loader<Data>::Ptr loader)
        : IResource(Type::eProject, id, manager)
        , loader_(std::move(loader))
        , initial_scene_(kInvalidResourceId)
        , action_bindings_({})
        , sensitivity_(0.0f)
    {}

    Project::~Project(){
        materials_.clear();
        action_bindings_.clear();
        RES_LOG_DESTRUCTION();
    }

    void Project::load() noexcept
    {
        assert(loader_ != nullptr);
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->path(id_, true);

        try
        {
            auto data = loader_->load(path);
            if (!data.has_value()){
                status_ = Status::eError;
                error_ = loader_->error();
                RES_LOG_ERROR(error_, "Failed to load project file:" + path);
                return;
            }

            materials_ = std::move(data.value().materials);
            action_bindings_ = std::move(data.value().action_bindings);
            sensitivity_ = data.value().sensitivity;

            if (!data.value().initial_scene_path.empty()){
                const auto rid = manager()->find(data.value().initial_scene_path);
                if (rid.has_value()){
                    initial_scene_ = rid.value();
                }else{
                    log_warn("Initial scene resource not found in list (" + data.value().initial_scene_path + ")");
                }
            }
        }
        catch (const std::exception& e){
            status_ = Status::eError;
            error_ = Error::eVulkanError;
            RES_LOG_ERROR(error_, e.what());
            return;
        }

        status_ = Status::eLoaded;
        error_ = Error::eNone;
        RES_LOG_LOADED();
    }
}
