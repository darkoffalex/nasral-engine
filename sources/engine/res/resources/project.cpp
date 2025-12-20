#include "pch.h"
#include <nasral/res/resources/project.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Project::Project(Manager* manager, const ResourceId id, Loader<Data>::Ptr loader)
        : IResource(Type::eProject, id, manager)
        , loader_(std::move(loader))
    {}

    Project::~Project(){
        materials_.clear();
        RES_LOG_DESTRUCTION();
    }

    void Project::load() noexcept
    {
        assert(loader_ != nullptr);
        if (status_ == Status::eLoaded) return;
        const auto path = manager()->path(id_, true);

        try
        {
            const auto data = loader_->load(path);
            if (!data.has_value()){
                status_ = Status::eError;
                error_ = loader_->error();
                RES_LOG_ERROR(error_, "Failed to load project file:" + path);
                return;
            }

            materials_ = std::move(data.value().materials);
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
