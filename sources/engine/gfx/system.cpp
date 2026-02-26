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
        update_lights_states();
        update_cam_uniforms();
    }

    void System::shutdown()
    {}

    void System::render() const
    {
        render_meshes();
    }

    void System::render_meshes() const
    {
        using Spatial = scn::comp::Spatial;
        using MeshNode = scn::comp::Mesh;
        using MeshHandles = comp::MeshHandles;
        using MatHandles = comp::MaterialHandles;
        using MatSettings = comp::MaterialSettings;
        using UboIdx = comp::UniformIndex;

        auto* renderer = engine()->renderer();
        auto* ecs = engine()->ecs();

        for (auto [e, spatial, mesh, mesh_hdl, mesh_ubo] : ecs->view<Spatial, MeshNode, MeshHandles, UboIdx>())
        {
            if (!mesh_hdl.mesh){
                log_warn("Mesh node has no render handles!");
                continue;
            }

            if (!mesh.material_entity.has_value()){
                log_warn("Mesh node has no material!");
                continue;
            }

            auto mat_e = mesh.material_entity.value();
            if (!ecs->is_valid(mat_e)
                || !ecs->has_component<MatSettings>(mat_e)
                || !ecs->has_component<MatHandles>(mat_e))
            {
                log_warn("Material entity is invalid!");
                continue;
            }

            const auto& mat_ubo = ecs->get_component<UboIdx>(mat_e);
            const auto& mat_hdl = ecs->get_component<MatHandles>(mat_e);

            renderer->cmd_bind_material(mat_hdl.material, mat_ubo.index);
            renderer->cmd_draw_mesh(mesh_hdl.mesh, mesh_ubo.index);
        }
    }

    void System::update_material_settings() const
    {
        using Dirty = comp::DirtyUniform;
        using Settings = comp::MaterialSettings;
        using UboIdx = comp::UniformIndex;

        // Сущности материалов с тегом MatDirtyTag должны обновить свои данные в SSBO/UBO рендерера
        for (auto [e, d, s, ubo] : engine()->ecs()->view<Dirty, Settings, UboIdx>())
        {
            if (s.type == MaterialType::ePhong){
                const auto* u = std::get_if<uniforms::MaterialPhong>(&s.uniforms);
                engine()->renderer()->update_mat_phong_uniforms(*u, ubo.index);
            }
            else if (s.type == MaterialType::ePbr){
                const auto* u = std::get_if<uniforms::MaterialPbr>(&s.uniforms);
                engine()->renderer()->update_mat_pbr_uniforms(*u, ubo.index);
            }

            engine()->ecs()->remove_component_deferred<Dirty>(e);
        }
    }

    void System::update_material_textures() const
    {
        using Dirty = comp::DirtyTextures;
        using Settings = comp::MaterialSettings;
        using Handles = comp::MaterialHandles;
        using UboIdx = comp::UniformIndex;

        for (auto [e, d, s, h, ubo] : engine()->ecs()->view<Dirty, Settings, Handles, UboIdx>())
        {
            for (const TextureType tt : magic_enum::enum_values<TextureType>())
            {
                if (!h.textures[tt]) continue;

                engine()->renderer()->update_mat_textures({
                    tt,
                    s.samplers[tt],
                    h.textures[tt]
                }, ubo.index);
            }
        }
    }

    void System::update_objects_uniforms() const
    {
        using Spatial = scn::comp::Spatial;
        using UboIdx = comp::UniformIndex;
        using State = comp::UniformState;
        using Cam = scn::comp::Camera;
        using Light = scn::comp::Light;

        for (auto [e, settings, ubo, state] : engine()->ecs()->view<Spatial, UboIdx, State>(ecs::kMaskOf<Cam, Light>))
        {
            if (!state.dirty) continue;

            uniforms::Object uniforms = {};
            auto& model = uniforms.model;
            auto& normals = uniforms.normals;

            model = glm::translate(model, settings.position);
            model = glm::rotate(model, glm::radians(settings.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::rotate(model, glm::radians(settings.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(settings.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, settings.scale);
            normals = glm::transpose(glm::inverse(glm::mat3(model)));

            engine()->renderer()->update_obj_uniforms(uniforms, ubo.index);
            state.dirty = false;
        }
    }

    void System::update_lights_uniforms() const
    {
        using Spatial = scn::comp::Spatial;
        using Light = scn::comp::Light;
        using UboIdx = comp::UniformIndex;
        using State = comp::UniformState;

        for (auto [e, spatial, light, ubo, state] : engine()->ecs()->view<Spatial, Light, UboIdx, State>())
        {
            if (!state.dirty) continue;

            uniforms::LightSettings uniforms = {};
            uniforms.type = static_cast<uint32_t>(light.type);
            uniforms.position = glm::vec4(spatial.position, 1.0f);
            uniforms.color = light.color;
            uniforms.intensity = light.intensity;
            uniforms.radius = light.radius;
            uniforms.quadratic = light.quadratic;

            engine()->renderer()->update_light_uniforms(uniforms, ubo.index);
            state.dirty = false;
        }
    }

    void System::update_lights_states() const
    {
        using Light = scn::comp::Light;
        using UboIdx = comp::UniformIndex;
        using Activate = comp::Activate;
        using Deactivate = comp::Deactivate;

        static std::vector<uint32_t> activate_lights;
        static std::vector<uint32_t> deactivate_lights;
        activate_lights.reserve(10);
        deactivate_lights.reserve(10);

        activate_lights.clear();
        for (auto [e, light, ubo, a] : engine()->ecs()->view<Light, UboIdx, Activate>())
        {
            activate_lights.push_back(ubo.index);
            engine()->ecs()->remove_component_deferred<Activate>(e);
        }

        deactivate_lights.clear();
        for (auto [e, light, ubo, d] : engine()->ecs()->view<Light, UboIdx, Deactivate>())
        {
            deactivate_lights.push_back(ubo.index);
            engine()->ecs()->remove_component_deferred<Deactivate>(e);
        }

        if (!activate_lights.empty()){
            engine()->renderer()->update_light_states_unsafe(activate_lights, true);
        }

        if (!deactivate_lights.empty()){
            engine()->renderer()->update_light_states_unsafe(deactivate_lights, false);
        }
    }

    void System::update_cam_uniforms() const
    {
        using Spatial = scn::comp::Spatial;
        using Cam = scn::comp::Camera;
        using UboIdx = comp::UniformIndex;
        using State = comp::UniformState;

        for (auto [e, spatial, cam, ubo, state] : engine()->ecs()->view<Spatial, Cam, UboIdx, State>())
        {
            if (!state.dirty) continue;

            // Матрица поворота камеры
            glm::mat4 cam_rotation =
                glm::rotate(glm::mat4(1.0f), glm::radians(spatial.rotation.y),glm::vec3(0.0f,1.0f,0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(spatial.rotation.x),glm::vec3(1.0f,0.0f,0.0f));

            // Матрица смещения камеры
            glm::mat4 cam_translate = glm::translate(glm::mat4(1.0f), spatial.position);

            uniforms::Camera uniforms = {};
            uniforms.position = glm::vec4(spatial.position, 1.0f);
            uniforms.view = glm::inverse(cam_translate * cam_rotation);
            uniforms.projection = glm::perspective(
                    glm::radians(cam.fov),
                    engine()->renderer()->get_rendering_aspect(),
                    cam.near,
                    cam.far);

            engine()->renderer()->update_cam_uniforms(uniforms, ubo.index);
            state.dirty = false;
        }
    }
}
