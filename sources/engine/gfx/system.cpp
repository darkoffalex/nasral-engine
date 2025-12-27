#include "pch.h"
#include <nasral/gfx/system.h>
#include <nasral/engine.h>
#include <nasral/ecs/view.h>
#include <nasral/ecs/manager.h>

namespace nasral::gfx
{
    System::System(Engine* engine) : ecs::System<System>(engine)
    {}

    System::~System()
    = default;

    void System::init()
    {}

    void System::update([[maybe_unused]] const float dt)
    {
        update_material_settings();
        update_material_textures();
    }

    void System::shutdown()
    {}

    void System::update_material_settings() const
    {
        using MatDirtyTag = comp::MaterialDirtySettings;
        using MatSettings = comp::MaterialSettings;

        // Сущности материалов с тегом MatDirtyTag должны обновить свои данные в SSBO/UBO рендерера
        for (auto [e, d, s] : engine()->ecs()->view<MatDirtyTag, MatSettings>())
        {
            if (s.type == MaterialType::ePhong){
                const auto* u = std::get_if<uniforms::MaterialPhong>(&s.uniforms);
                engine()->renderer()->update_mat_phong_uniforms(*u, s.index);
            }
            else if (s.type == MaterialType::ePbr){
                const auto* u = std::get_if<uniforms::MaterialPbr>(&s.uniforms);
                engine()->renderer()->update_mat_pbr_uniforms(*u, s.index);
            }

            engine()->ecs()->remove_component_deferred<MatDirtyTag>(e);
        }
    }

    void System::update_material_textures() const
    {
        using MatDirtyTag = comp::MaterialDirtyTextures;
        using MatSettings = comp::MaterialSettings;
        using MatHandles = comp::MaterialHandles;

        // Сущности материалов с тегом MatDirtyTag должны обновить свои данные текстурных дескрипторов
        for (auto [e, d, s, h] : engine()->ecs()->view<MatDirtyTag, MatSettings, MatHandles>())
        {
            for (const TextureType tt : magic_enum::enum_values<TextureType>())
            {
                if (!h.textures[tt]) continue;

                engine()->renderer()->update_mat_textures({
                    tt,
                    s.samplers[tt],
                    h.textures[tt]
                }, s.index);
            }

            engine()->ecs()->remove_component_deferred<MatDirtyTag>(e);
        }
    }
}
