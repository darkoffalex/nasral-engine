#pragma once

#include <nasral/res/objects/scene.h>

namespace nasral::res
{
    class SceneJsonLoader final : public Scene::Loader<Scene::Data>
    {
    public:
        explicit SceneJsonLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<Scene::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            Scene::Data data;

            set_error(Error::eNone);
            return std::optional{std::move(data)};
        }
    };
}