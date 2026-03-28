//
// Created by apant on 20/07/2025.
//

#include "MeshLoader.h"

#include <assimp/cimport.h> // Plain-C interface
#include <assimp/postprocess.h> // Post processing flags
#include <assimp/scene.h> // Output data structure
#include <stdexcept>

#include "Renderer.h"

namespace Renderer
{
void MeshLoader::Load(const char *file_path, BatchCpu *batch)
{
    const aiScene *scene = aiImportFile(
        file_path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices);

    if (!scene)
    {
        throw std::runtime_error("Failed to load mesh");
    }

    aiMatrix4x4 rotX;
    aiMatrix4x4::Rotation(glm::radians(0.0f), aiVector3D(1, 0, 0), rotX);

    aiMatrix4x4 rotY;
    aiMatrix4x4::Rotation(glm::radians(70.0f), aiVector3D(0, -1, 0), rotY);

    aiMatrix4x4 rotZ;
    aiMatrix4x4::Rotation(glm::radians(60.0f), aiVector3D(0, 0, 1), rotZ);

    ProcessNode(scene->mRootNode, scene, batch, rotX * rotY * rotZ);

    aiReleaseImport(scene);
}

void MeshLoader::ProcessNode(aiNode *node, const aiScene *scene, BatchCpu *batch,
    const aiMatrix4x4 &parentTransform)
{
    aiMatrix4x4 worldTransform = parentTransform * node->mTransformation;

    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, batch, worldTransform);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene, batch, worldTransform);
    }
}

void MeshLoader::ProcessMesh(aiMesh *mesh, const aiScene *scene, BatchCpu *batch,
    const aiMatrix4x4 &parentTransform)
{
    const uint32_t baseIndex = batch->position.size();

    for (uint32_t i = 0; i < mesh->mNumVertices; i++)
    {
        // Position
        aiVector3D originalPosition = mesh->mVertices[i];
        aiVector3D transformedPosition = parentTransform * originalPosition;
        glm::vec3 position = {};
        position.x = transformedPosition.x;
        position.y = transformedPosition.y;
        position.z = transformedPosition.z;
        batch->position.push_back(position);

        // Normals
        aiMatrix3x3 transformMatrix = aiMatrix3x3(parentTransform);
        aiVector3D orinalNormal = mesh->mNormals[i];
        aiVector3D transformedNormals = transformMatrix * orinalNormal;
        aiVector3Normalize(&transformedNormals);
        glm::vec3 normal = {};
        normal.x = transformedNormals.x;
        normal.y = transformedNormals.y;
        normal.z = transformedNormals.z;
        batch->normals.push_back(normal);
    }


    for (uint32_t i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];

        for (uint32_t j = 0; j < face.mNumIndices; j++)
        {
            batch->indices.push_back(baseIndex + face.mIndices[j]);
        }
    }
}

glm::mat4 MeshLoader::ConvertMatrix(const aiMatrix4x4 &aiMat)
{
    return glm::mat4x4(aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
        aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
        aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
        aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4);
}

void MeshLoader::QueryVerticesCount(
    const aiScene *scene,
    uint32_t *vertices_count)
{
    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        *vertices_count += scene->mMeshes[i]->mNumVertices;
    }
}

void MeshLoader::QueryVertecesPosition(
    const aiScene *scene,
    std::vector<glm::vec3> &positions)
{
    // Rotation of -90 degrees around X axis
    aiMatrix4x4 rotation;
    aiMatrix4x4::RotationX(-AI_MATH_PI / 2, rotation);

    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        for (size_t j = 0; j < scene->mMeshes[i]->mNumVertices; j++)
        {
            const aiVector3D &position = rotation * scene->mMeshes[i]->mVertices[j];
            positions.emplace_back(glm::vec3(position.x, position.y, position.z));
        }
    }
}

void MeshLoader::QueryVertecesNormal(
    const aiScene *scene,
    std::vector<glm::vec3> &normals)
{
    // Rotation of -90 degrees around X axis
    aiMatrix4x4 rotation;
    aiMatrix4x4::RotationX(-AI_MATH_PI / 2, rotation);

    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        for (size_t j = 0; j < scene->mMeshes[i]->mNumVertices; j++)
        {
            const aiVector3D &normal = rotation * scene->mMeshes[i]->mNormals[j];
            normals.emplace_back(glm::vec3(normal.x, normal.y, normal.z));
        }
    }
}

void MeshLoader::QueryIndicesCount(
    const aiScene *scene,
    uint32_t *indeces_count)
{
    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        *indeces_count += scene->mMeshes[i]->mNumFaces * 3;
    }
}

void MeshLoader::QueryIndices(
    const aiScene *scene,
    std::vector<uint32_t> &indices)
{
    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        for (size_t j = 0; j < scene->mMeshes[i]->mNumFaces; j++)
        {
            const aiFace &face = scene->mMeshes[i]->mFaces[j];
            indices.push_back(face.mIndices[0]);
            indices.push_back(face.mIndices[1]);
            indices.push_back(face.mIndices[2]);
        }
    }
}
}
