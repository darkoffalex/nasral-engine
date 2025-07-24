#pragma once
#include <nasral/resources/mesh.h>

namespace nasral::resources
{
    class MeshMockLoader final : public Loader<Mesh::Data>
    {
    public:
        std::optional<Mesh::Data> load([[maybe_unused]] const std::string_view& path) override{
            constexpr float size = 1.0f;
            std::vector<rendering::Vertex> vertices = {
                {{-(size/2.0f), -(size/2.0f), 0.0f},{0.0f, 0.0f, 1.0f},{0.0f, 0.0f},{1.0f, 0.0f,0.0f,1.0f}},
                {{0.0f, (size/2.0f), 0.0f},{0.0f, 0.0f, 1.0f},{0.5f, 1.0f},{0.0f, 1.0f,0.0f,1.0f}},
                {{(size/2.0f), -(size/2.0f), 0.0f},{0.0f, 0.0f, 1.0f},{1.0f, 0.0f},{0.0f, 0.0f,1.0f,1.0f}}
            };
            std::vector<uint32_t> indices = {0, 1, 2};
            return std::optional{Mesh::Data{
                std::move(vertices),
                std::move(indices)
            }};
        }
    };
}