#pragma once
#include <pugixml.hpp>
#include <magic_enum/magic_enum.hpp>
#include <nasral/res/resources/scene.h>

namespace nasral::res
{
    class SceneXmlLoader final : public Loader<Scene::Data>
    {
    public:
        explicit SceneXmlLoader(Engine* const engine) : Loader(engine)
        {}

        std::optional<Scene::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            Scene::Data data = {};

            // TODO: Загрузить из файла

            error_ = Error::eNone;
            return std::optional{std::move(data)};
        }
    };
}