#include "pch.h"
#include <nasral/res/objects/scene.h>
#include <nasral/res/manager.h>
#include <nasral/engine.h>

namespace nasral::res
{
    Scene::Scene(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader)
        : Resource(manager, id, Type::eScene)
        , loader_(std::move(loader))
    {}

    Scene::~Scene()
    {
        data_.nodes.clear();
        RES_LOG_DESTRUCTION();
    }

    void Scene::load() noexcept
    {
        assert(loader_ != nullptr && "Loader is null");
        if (status() == Status::eLoaded){
            return;
        }

        const auto full_path = subsystem()->path(id(), true);

        try
        {
            auto data = loader_->load(full_path);

            if (!data.has_value())
            {
                throw std::runtime_error("Failed to load scene file: " + full_path);
            }

            data_ = std::move(data.value());
        }
        catch (const std::exception& e){
            set_status(Status::eError);
            set_error(loader_->error());
            RES_LOG_ERROR(error(), e.what());
            return;
        }

        set_status(Status::eLoaded);
        set_error(Error::eNone);
        RES_LOG_LOADED();
    }
}