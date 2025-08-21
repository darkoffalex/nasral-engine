#pragma once
#include <nasral/resources/mesh.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace nasral::resources
{
    class MeshLoader final : public Loader<Mesh::Data>
    {
    public:
        explicit MeshLoader(const std::optional<LoadParams>& params = std::nullopt) : Loader(params){
            if (load_params_.has_value()){
                assert(std::get_if<MeshLoadParams>(&load_params_.value()) != nullptr);
            }else{
                load_params_ = MeshLoadParams();
            }
        }

        std::optional<Mesh::Data> load([[maybe_unused]] const std::string_view& path) override{
            Assimp::Importer importer;

            // Флаги пост-обработки (триангулировать, генерация нормалей, соединять идентичные вершины)
            unsigned int flags
                = aiProcess_Triangulate
                | aiProcess_JoinIdenticalVertices;

            // Порядок обхода вершин
            if (auto* lp = load_params<MeshLoadParams>()) {
                if (!lp->winding_ccw) {
                    flags |= aiProcess_FlipWindingOrder;
                }
                if (lp->gen_normals) {
                    flags |= aiProcess_GenSmoothNormals;
                }
                if (lp->gen_tangents) {
                    flags |= aiProcess_CalcTangentSpace;
                }
            }

            // Загрузка сцены из файла
            const aiScene* scene = importer.ReadFile(path.data(), flags);
            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
                err_code_ = ErrorCode::eLoadingError;
                return std::nullopt;
            }

            // Сцена должна содержать mesh'ы
            if (!scene->HasMeshes()){
                err_code_ = ErrorCode::eBadFormat;
                return std::nullopt;
            }

            // Итоговые массивы вершин и индексов
            std::vector<rendering::Vertex> vertices;
            std::vector<uint32_t> indices;

            // Проход по всем mesh'ам
            for (unsigned int mesh_idx = 0; mesh_idx < scene->mNumMeshes; mesh_idx++){
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
                    rendering::Vertex vertex = {};

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
                        err_code_ = ErrorCode::eBadFormat;
                        return std::nullopt;
                    }

                    for (unsigned int idx = 0; idx < face.mNumIndices; idx++){
                        indices.push_back(face.mIndices[idx]);
                    }
                }
            }

            if (vertices.empty() || indices.empty()){
                err_code_ = ErrorCode::eBadFormat;
                return std::nullopt;
            }

            err_code_ = ErrorCode::eNoError;
            return std::optional{Mesh::Data{
                std::move(vertices),
                std::move(indices)
            }};
        }
    };
}