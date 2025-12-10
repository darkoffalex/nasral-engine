#pragma once
#include <nasral/res/resources/mesh.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace nasral::res
{
    class MeshAssimpLoader final : public Loader<Mesh::Data>
    {
    public:
        explicit MeshAssimpLoader(const LoadParamsOpt& params = std::nullopt) : Loader(params)
        {
            if (params.has_value()){
                assert(std::holds_alternative<MeshLoadParams>(params.value()));
            }else{
                load_params_ = MeshLoadParams{};
            }
        }

        std::optional<Mesh::Data> load(const std::string_view& path) override
        {
            Assimp::Importer importer;

            // Флаги пост-обработки (триангуляция, идентичные вершины)
            unsigned int flags
                = aiProcess_Triangulate
                | aiProcess_JoinIdenticalVertices;

            // Флаги пост-обработки (порядок обхода, генерация нормалей, генерация касательных)
            if (auto* lp = load_params<MeshLoadParams>()) {
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
                error_ = Error::eLoadingFailed;
                return std::nullopt;
            }

            // Сцена должна содержать meshes
            if (!scene->HasMeshes()){
                error_ = Error::eBadFormat;
                return std::nullopt;
            }

            // Итоговые массивы вершин и индексов
            std::vector<gfx::Vertex> vertices;
            std::vector<uint32_t> indices;

            // Проход по всем mesh-ам
            for (unsigned int mesh_idx = 0; mesh_idx < scene->mNumMeshes; mesh_idx++)
            {
                const aiMesh* mesh = scene->mMeshes[mesh_idx];
                if (!mesh->HasPositions() || !mesh->HasFaces()){
                    continue;
                }

                // Выделить нужную память
                vertices.reserve(vertices.size() + mesh->mNumVertices);
                indices.reserve(indices.size() + mesh->mNumFaces * 3);

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

                // Добавить индексы
                for (unsigned int face_idx = 0; face_idx < mesh->mNumFaces; face_idx++){
                    const aiFace& face = mesh->mFaces[face_idx];
                    if (face.mNumIndices != 3){
                        error_ = Error::eBadFormat;
                        return std::nullopt;
                    }

                    for (unsigned int idx = 0; idx < face.mNumIndices; idx++){
                        indices.push_back(face.mIndices[idx]);
                    }
                }
            }

            if (vertices.empty() || indices.empty()){
                error_ = Error::eBadFormat;
                return std::nullopt;
            }


            error_ = Error::eNone;
            return std::optional{Mesh::Data{
                std::move(vertices),
                std::move(indices)
            }};
        }
    };
}