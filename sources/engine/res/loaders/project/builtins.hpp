#pragma once
#include <nasral/res/resources/project.h>
#include <nasral/gfx/types.h>

namespace nasral::res
{
    class ProjectBuiltinLoader final : public Loader<Project::Data>
    {
    public:
        std::optional<Project::Data> load(const std::string_view& path) override
        {
            if (path.find(kBuiltinProjectFile) != std::string::npos)
            {
                // Список материалов по умолчанию
                std::vector<gfx::io::Material> materials = {
                    {
                        core::UniqueId(),
                        gfx::MaterialType::eDummy,
                        "materials/dummy/material.xml",
                        {},
                        {}
                    },
                    {
                        core::UniqueId(),
                        gfx::MaterialType::eVertexColored,
                        "materials/vertex-colored/material.xml",
                        {},
                        {}
                    },
                    {
                        core::UniqueId(),
                        gfx::MaterialType::ePhong,
                        "materials/phong/material.xml",
                        {
                            "textures/chair/chair_diff_1k.png:v0",
                            "textures/chair/chair_nor_gl_1k.png",
                            "textures/chair/chair_spec_1k.png"
                        },
                        {}
                    },
                    {
                        core::UniqueId(),
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
                        {}
                    }
                };

                // Уникальные ID соответствуют порядковому номеру в списке
                for (size_t i = 0; i < materials.size(); ++i){
                    materials[i].id.set(0, i);
                }

                return std::optional{
                    Project::Data{std::move(materials)}
                };
            }

            return std::nullopt;
        }
    };
}
