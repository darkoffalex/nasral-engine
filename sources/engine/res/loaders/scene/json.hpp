#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include <nasral/res/objects/scene.h>
#include <nasral/scn/types.h>
#include <magic_enum/magic_enum.hpp>
#include <glm/glm.hpp>

namespace nasral::res
{
    class SceneJsonLoader final : public Scene::Loader<Scene::Data>
    {
    public:
        explicit SceneJsonLoader(Manager* manager) : Loader(manager)
        {}

        std::optional<Scene::Data> load(const std::string_view& path) override
        {
            Scene::Data data;

            try
            {
                std::ifstream file(path.data());
                if (!file.is_open()){
                    throw std::runtime_error("Failed to open file");
                }

                nlohmann::json json;
                file >> json;

                // Узлы
                for (const auto& node_entry : json.at("nodes")){
                    data.nodes.push_back(parse_node_entry(node_entry));
                }

                // Активный эффект экрана (временный hard code)
                // TODO: Читать из файла
                data.screen_fx_settings.default_fx_uid = UniqueId{2, 0};

                file.close();
            }
            catch ([[maybe_unused]] const nlohmann::json::exception& e){
                set_error(Error::eBadFormat);
                return std::nullopt;
            }
            catch ([[maybe_unused]] std::exception& e){
                set_error(Error::eLoadingFailed);
                return std::nullopt;
            }

            set_error(Error::eNone);
            return std::optional{std::move(data)};
        }

    private:
        static scn::NodeDesc parse_node_entry(const nlohmann::json& entry)
        {
            scn::NodeDesc desc = {};
            desc.name = entry.at("name").get<std::string>();

            const auto type_str = entry.at("type").get<std::string>();
            desc.type = magic_enum::enum_cast<scn::NodeType>(type_str).value_or(scn::NodeType::eDummy);

            if (const auto& uid_array = entry.at("uid").get<std::vector<uint64_t>>(); uid_array.size() >= 2)
            {
                desc.unique_id.set(uid_array[0], uid_array[1]);
            }

            // Spatial
            if (entry.contains("spatial"))
            {
                const auto& s = entry.at("spatial");
                desc.spatial.position = parse_vec3(s.at("position"));
                desc.spatial.rotation = parse_vec3(s.at("rotation"));
                desc.spatial.scale = parse_vec3(s.at("scale"));
            }

            // Camera
            if (desc.type == scn::NodeType::eCamera && entry.contains("camera"))
            {
                const auto& c = entry.at("camera");
                const auto view_type_str = c.at("type").get<std::string>();
                desc.camera.type = magic_enum::enum_cast<gfx::ViewType>(view_type_str).value_or(gfx::ViewType::ePerspective);
                desc.camera.fov = c.at("fov").get<float>();
                desc.camera.aspect = c.at("aspect").get<float>();
                desc.camera.near = c.at("near").get<float>();
                desc.camera.far = c.at("far").get<float>();
            }

            // Mesh
            if (desc.type == scn::NodeType::eMesh && entry.contains("mesh"))
            {
                const auto& m = entry.at("mesh");
                desc.mesh.mesh_path = m.at("mesh_path").get<std::string>();
                for (const auto& mat_uid : m.at("materials"))
                {
                    if (const auto& uid_arr = mat_uid.get<std::vector<uint64_t>>(); uid_arr.size() >= 2)
                    {
                        desc.mesh.materials.emplace_back(uid_arr[0], uid_arr[1]);
                    }
                }
            }

            // Light
            if (desc.type == scn::NodeType::eLight && entry.contains("light"))
            {
                const auto& l = entry.at("light");
                const auto light_type_str = l.at("type").get<std::string>();
                desc.light.dynamic = l.at("dynamic").get<bool>();
                desc.light.type = magic_enum::enum_cast<gfx::LightType>(light_type_str).value_or(gfx::LightType::ePointLight);
                desc.light.intensity = l.at("intensity").get<float>();
                desc.light.radius = l.at("radius").get<float>();
                desc.light.quadratic = l.at("quadratic").get<float>();
                desc.light.color = parse_vec4(l.at("color"));
            }

            return desc;
        }

        static glm::vec3 parse_vec3(const nlohmann::json& j)
        {
            return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
        }

        static glm::vec4 parse_vec4(const nlohmann::json& j)
        {
            return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
        }
    };
}
