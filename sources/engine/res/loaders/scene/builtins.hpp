#pragma once
#include <nasral/res/resources/scene.h>
#include <nasral/scn/io/base_node.h>
#include <nasral/scn/io/mesh_node.h>

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

            // Корень сцены
            auto root = std::make_unique<scn::io::Node>(
                engine(),
                scn::io::Node::Data{
                    core::UniqueId(1, 0),
                    scn::NodeType::eDummy,
                    {}
                });

            // Добавить потомков
            root->io_node_data.children.emplace_back(std::make_unique<scn::io::MeshNode>(
                engine(),
                scn::io::MeshNode::DataTuple
                {
                    scn::io::Node::Data{
                        core::UniqueId(1, 1),
                        scn::NodeType::eMesh,
                        {}
                    },
                    scn::io::SpatialNode::Data{
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(1.2f),
                        glm::vec3(0.0f)
                    },
                    scn::io::MeshNode::Data{
                        "meshes/chair/chair.obj",
                        core::UniqueId(0, 2) // Цветные вершины (UID 02)
                    }
                }));

            error_ = Error::eNone;
            return std::optional{Scene::Data{
                std::move(root),
            }};
        }
    };
}
