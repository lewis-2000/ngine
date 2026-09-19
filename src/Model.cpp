#include "Model.h"

#include <assimp/postprocess.h>

#include <iostream>
#include <utility>

Model::Model(const std::string &path)
{
    const aiScene *scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs);

    if (!scene || !scene->mRootNode)
    {
        std::cerr << "Failed to load model: " << importer.GetErrorString() << '\n';
        return;
    }

    processNode(scene->mRootNode, scene);
}

void Model::Draw(Shader &shader)
{
    for (Mesh &mesh : meshes)
    {
        mesh.Draw(shader);
    }
}

void Model::processNode(aiNode *node, const aiScene *scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh *mesh)
{
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex vertex;
        vertex.Position = {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z};

        if (mesh->HasNormals())
        {
            vertex.Normal = {
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z};
        }

        if (mesh->mTextureCoords[0])
        {
            vertex.TexCoords = {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y};
        }

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        const aiFace &face = mesh->mFaces[i];

        for (unsigned int j = 0; j < face.mNumIndices; ++j)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    return Mesh(
        std::move(vertices),
        std::move(indices),
        {},
        mesh->mName.C_Str(),
        true);
}