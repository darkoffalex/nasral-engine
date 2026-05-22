#pragma once

#include <nasral/res/objects/mesh.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace nasral::res
{
    class MeshAssimpLoader final : public Mesh::Loader<Mesh::Data>
    {
    public:
        explicit MeshAssimpLoader(Manager* manager, const std::optional<LoadParams>& params = std::nullopt)
            : Loader(manager, params)
        {
            if (params.has_value()){
                assert(std::holds_alternative<MeshLoadParams>(params.value()) && "Wrong loading params");
            }else{
                set_params(MeshLoadParams{});
            }
        }

        std::optional<Mesh::Data> load([[maybe_unused]] const std::string_view& path) override
        {
            Assimp::Importer importer;

            // Флаги пост-обработки (триангуляция, идентичные вершины)
            unsigned int flags
                = aiProcess_Triangulate
                | aiProcess_JoinIdenticalVertices;

            // Флаги пост-обработки (порядок обхода, генерация нормалей, генерация касательных)
            if (auto* lp = params<MeshLoadParams>()) {
                if (!lp->winding_order_ccw) {
                    flags |= aiProcess_FlipWindingOrder;
                }
                if (lp->generate_normals) {
                    flags |= aiProcess_GenSmoothNormals;
                }
                if (lp->generate_tangents) {
                    flags |= aiProcess_CalcTangentSpace;
                }
            }

            // Загрузка сцены из файла
            const aiScene* scene = importer.ReadFile(path.data(), flags);
            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
                set_error(Error::eLoadingFailed);
                return std::nullopt;
            }

            // Сцена должна содержать meshes
            if (!scene->HasMeshes()){
                set_error(Error::eBadFormat);
                return std::nullopt;
            }

            // Группировка индексов по материалу
            std::map<uint32_t, std::vector<uint32_t>> material_indices_map;
            std::vector<gfx::Vertex> vertices;

            // Проход по всем mesh-ам
            for (unsigned int mesh_idx = 0; mesh_idx < scene->mNumMeshes; mesh_idx++)
            {
                const aiMesh* mesh = scene->mMeshes[mesh_idx];
                if (!mesh->HasPositions() || !mesh->HasFaces()){
                    continue;
                }

                const auto vertex_offset = static_cast<uint32_t>(vertices.size());
                vertices.reserve(vertices.size() + mesh->mNumVertices);

                // Добавить вершины
                for (unsigned int vtx_idx = 0; vtx_idx < mesh->mNumVertices; vtx_idx++)
                {
                    gfx::Vertex vertex = {};

                    vertex.pos = glm::vec3(
                        mesh->mVertices[vtx_idx].x,
                        mesh->mVertices[vtx_idx].y,
                        mesh->mVertices[vtx_idx].z);

                    vertex.normal = glm::vec3(
                        mesh->mNormals[vtx_idx].x,
                        mesh->mNormals[vtx_idx].y,
                        mesh->mNormals[vtx_idx].z);

                    if (mesh->HasTextureCoords(0)){
                        vertex.uv = glm::vec2(
                            mesh->mTextureCoords[0][vtx_idx].x,
                            mesh->mTextureCoords[0][vtx_idx].y);
                    }else{
                        vertex.uv = glm::vec2(0.0f, 0.0f);
                    }

                    if (mesh->HasVertexColors(0)){
                        vertex.color = glm::vec4(
                            mesh->mColors[0][vtx_idx].r,
                            mesh->mColors[0][vtx_idx].g,
                            mesh->mColors[0][vtx_idx].b,
                            1.0f);
                    }else{
                        vertex.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                    }

                    vertices.push_back(vertex);
                }

                // Собрать индексы с учетом offset вершин
                std::vector<uint32_t> local_indices;
                local_indices.reserve(mesh->mNumFaces * 3);
                for (unsigned int face_idx = 0; face_idx < mesh->mNumFaces; face_idx++){
                    const aiFace& face = mesh->mFaces[face_idx];
                    if (face.mNumIndices != 3){
                        set_error(Error::eBadFormat);
                        return std::nullopt;
                    }

                    for (unsigned int idx = 0; idx < face.mNumIndices; idx++){
                        local_indices.push_back(vertex_offset + face.mIndices[idx]);
                    }
                }

                // Добавить в группу по материалу
                material_indices_map[mesh->mMaterialIndex].insert(
                    material_indices_map[mesh->mMaterialIndex].end(),
                    local_indices.begin(),
                    local_indices.end());
            }

            if (vertices.empty()){
                set_error(Error::eBadFormat);
                return std::nullopt;
            }

            // Собрать финальные индексы и surfaces
            std::vector<uint32_t> indices;
            std::vector<Mesh::Surface> surfaces;
            uint32_t current_offset = 0;
            for (const auto& [mat_idx, mat_indices] : material_indices_map) {
                if (!mat_indices.empty()) {
                    surfaces.push_back({
                        current_offset,
                        static_cast<uint32_t>(mat_indices.size()),
                        mat_idx
                    });
                    indices.insert(indices.end(), mat_indices.begin(), mat_indices.end());
                    current_offset += static_cast<uint32_t>(mat_indices.size());
                }
            }


            set_error(Error::eNone);
            return std::optional{Mesh::Data{
                std::move(vertices),
                std::move(indices),
                std::move(surfaces)}
            };
        }
    };
}