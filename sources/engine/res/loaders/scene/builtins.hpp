#pragma once
#include <nasral/res/resources/scene.h>
#include <nasral/scn/io/base_node.h>
#include <nasral/scn/io/mesh_node.h>
#include <nasral/scn/io/light_node.h>

namespace nasral::res
{
    class SceneBuiltinLoader final : public Loader<Scene::Data>
    {
    public:
        explicit SceneBuiltinLoader(Engine* const engine) : Loader(engine)
        {}

        std::optional<Scene::Data> load(const std::string_view& path) override{
            if (path.find(kBuiltinSceneDefault) == std::string::npos){
                return std::nullopt;
            }

            // Корень сцены (UID 10)
            auto root = std::make_unique<scn::io::Node>(
                engine(),
                scn::io::Node::Data{
                    core::UniqueId(1, 0),
                    scn::NodeType::eDummy,
                    {}
                });

            // Камера (добавлено неявно - UID 11)

            // Потомок - меш (UID 12)
            root->io_node_data.children.emplace_back(std::make_unique<scn::io::MeshNode>(
                engine(),
                scn::io::MeshNode::DataTuple
                {
                    scn::io::Node::Data{
                        core::UniqueId(1, 2),
                        scn::NodeType::eMesh,
                        {}
                    },
                    scn::io::SpatialNode::Data{
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(1.2f),
                        glm::vec3(0.0f)
                    },
                    scn::io::MeshNode::Data{
                        "meshes/chair/chair.obj", // Меш
                        core::UniqueId(0, 4)      // Материал: Phong (UID 03)
                    }
                }));

            // Потомок - источник света (UID 13)
            root->io_node_data.children.emplace_back(std::make_unique<scn::io::LightNode>(
                engine(),
                scn::io::LightNode::DataTuple
                {
                    scn::io::Node::Data{
                        core::UniqueId(1, 3),
                        scn::NodeType::eLight,
                        {}
                    },
                    scn::io::SpatialNode::Data{
                        glm::vec3(-1.0f, 1.0f, 2.5f),
                        glm::vec3(1.0f),
                        glm::vec3(0.0f)
                    },
                    scn::io::LightNode::Data{
                        gfx::LightType::ePointLight,
                        2.0f,
                        1.0f,
                        0.1f,
                        glm::vec4(1.0f)
                    }
                }));

            // Потомок - источник света (UID 14)
            root->io_node_data.children.emplace_back(std::make_unique<scn::io::LightNode>(
                engine(),
                scn::io::LightNode::DataTuple
                {
                    scn::io::Node::Data{
                        core::UniqueId(1, 4),
                        scn::NodeType::eLight,
                        {}
                    },
                    scn::io::SpatialNode::Data{
                        glm::vec3(1.0f, 1.0f, 2.5f),
                        glm::vec3(1.0f),
                        glm::vec3(0.0f)
                    },
                    scn::io::LightNode::Data{
                        gfx::LightType::ePointLight,
                        2.0f,
                        1.0f,
                        0.1f,
                        glm::vec4(1.0f)
                    }
                }));

            error_ = Error::eNone;
            return std::optional{Scene::Data{
                std::move(root),
            }};
        }
    };
}
