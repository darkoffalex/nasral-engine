#pragma once
#include <nasral/res/resources/project.h>
#include <nasral/gfx/types.h>

namespace nasral::res
{
    class ProjectBuiltinLoader final : public Loader<Project::Data>
    {
    public:
        explicit ProjectBuiltinLoader(Engine* const engine) : Loader(engine)
        {}

        std::optional<Project::Data> load(const std::string_view& path) override
        {
            if (path.find(kBuiltinProjectFile) != std::string::npos)
            {
                // Список материалов по умолчанию
                std::vector<gfx::io::Material::Ptr> materials = {};
                materials.reserve(4);

                // Dummy (UID 01)
                materials.emplace_back(std::make_unique<gfx::io::Material>(
                    engine(),
                    gfx::io::Material::Data
                    {
                        core::UniqueId(0, 1),
                        gfx::MaterialType::eDummy,
                        "materials/dummy/material.xml",
                        {},
                        {},
                        {}
                    }
                ));

                // Цветные вершины (UID 02)
                materials.emplace_back(std::make_unique<gfx::io::Material>(
                    engine(),
                    gfx::io::Material::Data
                    {
                        core::UniqueId(0, 2),
                        gfx::MaterialType::eVertexColored,
                        "materials/vertex-colored/material.xml",
                        {},
                        {},
                        {}
                    }
                ));

                // Освещение Phong (UID 03)
                materials.emplace_back(std::make_unique<gfx::io::Material>(
                    engine(),
                    gfx::io::Material::Data
                    {
                        core::UniqueId(0, 3),
                        gfx::MaterialType::ePhong,
                        "materials/phong/material.xml",
                        {
                            "textures/chair/chair_diff_1k.png:v0",
                            "textures/chair/chair_nor_gl_1k.png",
                            "textures/chair/chair_spec_1k.png"
                        },
                        {
                            gfx::TextureSamplerType::eLinear,
                            gfx::TextureSamplerType::eLinear,
                            gfx::TextureSamplerType::eLinear,
                        },
                        gfx::uniforms::MaterialPhong{}
                    }
                ));

                // Освещение PBR (UID 04)
                materials.emplace_back(std::make_unique<gfx::io::Material>(
                    engine(),
                    gfx::io::Material::Data
                    {
                        core::UniqueId(0, 4),
                        gfx::MaterialType::ePbr,
                        "materials/pbr/material.xml",
                        {
                            "textures/chair/chair_diff_1k.png:v1",
                            "textures/chair/chair_nor_gl_1k.png",
                            "textures/chair/chair_rough_1k.png",
                            "builtin:tex/white-pixel",
                            "textures/chair/chair_metal_1k.png",
                            "builtin:tex/white-pixel"
                        },
                        {
                            gfx::TextureSamplerType::eLinear,
                            gfx::TextureSamplerType::eLinear,
                            gfx::TextureSamplerType::eLinear,
                            gfx::TextureSamplerType::eLinear,
                            gfx::TextureSamplerType::eLinear,
                            gfx::TextureSamplerType::eLinear,
                        },
                        gfx::uniforms::MaterialPbr{}
                    }
                ));

                error_ = Error::eNone;
                return std::optional{Project::Data{
                    // Список материалов
                    std::move(materials),
                    // Путь к изначальной сцене
                    kBuiltinSceneDefault.data(),
                    // Настройки управления (действия)
                    {
                        {"forward", {inp::KeyCode::eW}},
                        {"backward", {inp::KeyCode::eS}},
                        {"left", {inp::KeyCode::eA}},
                        {"right", {inp::KeyCode::eD}},
                        {"up", {inp::KeyCode::eSpace}},
                        {"down", {inp::KeyCode::eC}},
                        {"exit", {inp::KeyCode::eEscape}}
                    },
                    // Чувствительность мыши
                    1.0f
                }};
            }

            return std::nullopt;
        }
    };
}
