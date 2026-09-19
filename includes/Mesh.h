#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "Texture.h"
#include "Shader.h"

// Each vertex contains position, normal, texCoords, bone data
struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::ivec4 BoneIDs = glm::ivec4(-1);     // Up to 4 bones per vertex
    glm::vec4 BoneWeights = glm::vec4(0.0f); // Weights for each bone
};

struct MeshMaterialOverride
{
    enum class Mode
    {
        Default = 0,
        Color = 1,
        Texture = 2
    };

    Mode mode = Mode::Default;
    glm::vec3 color = glm::vec3(1.0f);
    std::string texturePath;
};

#define MAX_BONE_INFLUENCE 4

class Mesh
{
public:
    // Mesh data
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::vector<Texture> textures;
    std::string name;

    // OpenGL objects
    VAO vao;
    VBO *vbo;
    EBO *ebo;

    Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures, std::string name = std::string(), bool deferSetup = false);

    void Draw(Shader &shader, const MeshMaterialOverride *overrideMaterial = nullptr);
    void InitializeGLObjects(); // Call this on main thread after creation
    static void ClearTextureCache();
    bool IsInitialized() const { return m_GLObjectsInitialized; }
    const std::string &GetName() const { return name; }

private:
    void setupMesh();
    bool m_GLObjectsInitialized = false;
};
