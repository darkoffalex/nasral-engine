#pragma once
#include <nasral/res/resources/scene.h>

namespace nasral::res
{
    class SceneBuiltinLoader final : public Loader<Scene::Data>
    {
    public:
        std::optional<Scene::Data> load(const std::string_view& path) override
        {
            if (path.find(kBuiltinSceneDefault) == std::string::npos)
            {
                return std::nullopt;
            }

            // TODO: Подготовить сцену

            return std::nullopt;
        }
    };
}
