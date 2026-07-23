#pragma once

#include <nasral/res/objects/scene.h>
#include <nasral/scn/types.h>

namespace nasral::res
{
    class SceneBuiltinLoader final : public Scene::Loader<Scene::Data>
    {
    public:
        explicit SceneBuiltinLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<Scene::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            Scene::Data data;

            if (path.find(kBuiltinSceneDefault) != std::string::npos)
            {
                // Камера
                scn::NodeDesc camera_desc = {};
                camera_desc.type = scn::NodeType::eCamera;
                camera_desc.name = "Camera";
                camera_desc.unique_id = UniqueId::generate();
                camera_desc.spatial.position = {0.0f, 0.0f, 2.0f};
                camera_desc.spatial.scale = {1.0f, 1.0f, 1.0f};
                camera_desc.spatial.rotation = {0.0f, 0.0f, 0.0f};
                camera_desc.camera.type = gfx::ViewType::ePerspective;
                camera_desc.camera.fov = 90.0f;
                camera_desc.camera.aspect = 1.0f;
                camera_desc.camera.near = 0.1f;
                camera_desc.camera.far = 1000.0f;
                data.nodes.push_back(camera_desc);

                // Меш (куб)
                scn::NodeDesc cube_desc = {};
                cube_desc.type = scn::NodeType::eMesh;
                cube_desc.name = "Cube";
                cube_desc.unique_id = UniqueId::generate();
                cube_desc.spatial.position = {0.0f, 0.0f, 0.0f};
                cube_desc.spatial.scale = {1.0f, 1.0f, 1.0f};
                cube_desc.spatial.rotation = {0.0f, 0.0f, 0.0f};
                cube_desc.mesh.mesh_path = kBuiltinMeshCube;
                cube_desc.mesh.materials = {UniqueId{0,1}};
                data.nodes.push_back(cube_desc);

                // Источник света
                scn::NodeDesc light_desc = {};
                light_desc.type = scn::NodeType::eLight;
                light_desc.name = "Light";
                light_desc.unique_id = UniqueId::generate();
                light_desc.spatial.position = {0.0f, 0.3f, 2.0f};
                light_desc.spatial.scale = {1.0f, 1.0f, 1.0f};
                light_desc.spatial.rotation = {0.0f, 0.0f, 0.0f};
                light_desc.light.dynamic = false;
                light_desc.light.is_active = true;
                light_desc.light.radius = 1.0f;
                light_desc.light.type = gfx::LightType::ePointLight;
                light_desc.light.color = glm::vec4(1.0f);
                light_desc.light.intensity = 1.0f;
                light_desc.light.quadratic = 0.1f;
                data.nodes.push_back(light_desc);
            }

            set_error(Error::eNone);
            return std::optional{std::move(data)};
        }
    };
}