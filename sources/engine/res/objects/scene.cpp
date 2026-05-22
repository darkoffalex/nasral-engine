#include "pch.h"
#include <nasral/res/objects/scene.h>

namespace nasral::res
{
    Scene::Scene(Manager* manager, const ResourceId& id, Loader<Data>::Ptr loader)
        : Resource(manager, id, Type::eScene)
        , loader_(std::move(loader))
    {
    }

    Scene::~Scene()
    {
    }

    void Scene::load() noexcept
    {
    }
}