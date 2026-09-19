#pragma once

#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <string>
#include <vector>

class Model
{
public:
    explicit Model(const std::string &path);

    void Draw(Shader &shader);
    std::size_t meshCount() const { return meshes.size(); }

private:
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh);

    std::vector<Mesh> meshes;
    Assimp::Importer importer;
};