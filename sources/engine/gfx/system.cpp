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
        update_objects_uniforms();
        update_lights_uniforms();
        update_cam_uniforms();
    }

    void System::shutdown()
    {}

    void System::render() const
    {
        using Mesh = scn::comp::Mesh;
        using Spatial = scn::comp::Spatial;
        using MeshHandles = comp::MeshHandles;
        using MatHandles = comp::MaterialHandles;
        using MatSettings = comp::MaterialSettings;

        auto* renderer = engine()->renderer();
        auto* ecs = engine()->ecs();

        for (auto [e, m, s, h] : ecs->view<Mesh, Spatial, MeshHandles>())
        {
            if (!m.material_entity.has_value()){
                log_warn("Mesh has no material!");
                continue;
            }

            if (!ecs->is_valid(m.material_entity.value())
                || !ecs->has_component<MatSettings>(m.material_entity.value())
                || !ecs->has_component<MatHandles>(m.material_entity.value()))
            {
                log_warn("Material entity is invalid!");
                continue;
            }

            if (!h.mesh){
                log_warn("Mesh has no render handles!");
                continue;
            }

            const auto& mts = ecs->get_component<MatSettings>(m.material_entity.value());
            const auto& mth = ecs->get_component<MatHandles>(m.material_entity.value());

            renderer->cmd_bind_material(mth.material, mts.index);
            renderer->cmd_draw_mesh(h.mesh, s.obj_index);
        }
    }

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

    void System::update_objects_uniforms() const
    {
        using Mesh = scn::comp::Mesh;
        using Spatial = scn::comp::Spatial;
        using Dirty = scn::comp::SpatialDirty;

        for (auto [e, m, s, d] : engine()->ecs()->view<Mesh, Spatial, Dirty>())
        {
            uniforms::Object uniforms = {};
            auto& model = uniforms.model;
            auto& normals = uniforms.normals;

            model = glm::translate(model, s.position);
            model = glm::rotate(model, glm::radians(s.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(s.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(s.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, s.scale);
            normals = glm::transpose(glm::inverse(glm::mat3(model)));

            engine()->renderer()->update_obj_uniforms(uniforms, s.obj_index);
            engine()->ecs()->remove_component_deferred<Dirty>(e);
        }
    }

    void System::update_lights_uniforms() const
    {
        // TODO: Implement
    }

    void System::update_cam_uniforms() const
    {
        using Cam = scn::comp::Camera;
        using Spatial = scn::comp::Spatial;
        using Dirty = scn::comp::CameraDirty;

        for (auto [e, c, s, d] : engine()->ecs()->view<Cam, Spatial, Dirty>())
        {
            uniforms::Camera uniforms = {};
            uniforms.position = glm::vec4(s.position, 1.0f);
            uniforms.view = glm::translate(glm::mat4(1.0f), -glm::vec3(s.position));
            uniforms.projection = glm::perspective(
                    glm::radians(c.fov),
                    engine()->renderer()->get_rendering_aspect(),
                    c.near,
                    c.far);

            engine()->renderer()->update_cam_uniforms(uniforms, c.obj_index);
            engine()->ecs()->remove_component_deferred<Cam>(e);
        }
    }
}
