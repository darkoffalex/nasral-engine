#pragma once
#include <nasral/resources/mesh.h>

namespace nasral::resources
{
    class MeshLoader final : public Loader<Mesh::Data>
    {
    public:
        std::optional<Mesh::Data> load([[maybe_unused]] const std::string_view& path) override{
            // TODO: Реализовать загрузку при помощи assimp
            return std::nullopt;
        }
    };
}