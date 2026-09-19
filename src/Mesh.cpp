#include "Mesh.h"
#include <stb_image.h>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace
{
    std::unordered_map<std::string, GLuint> g_TextureCache;
    std::unordered_set<std::string> g_FailedTextureLoads;

    bool UploadTextureIfNeeded(Texture &texture)
    {
        if (texture.ID != 0 || texture.path.empty())
            return texture.ID != 0;

        auto cached = g_TextureCache.find(texture.path);
        if (cached != g_TextureCache.end())
        {
            texture.ID = cached->second;
            return true;
        }

        if (g_FailedTextureLoads.find(texture.path) != g_FailedTextureLoads.end())
            return false;

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_set_flip_vertically_on_load(true);
        unsigned char *data = stbi_load(texture.path.c_str(), &width, &height, &channels, 0);
        if (!data)
        {
            g_FailedTextureLoads.insert(texture.path);
            return false;
        }

        GLenum format = GL_RGB;
        if (channels == 1)
            format = GL_RED;
        else if (channels == 4)
            format = GL_RGBA;

        glGenTextures(1, &texture.ID);
        glBindTexture(GL_TEXTURE_2D, texture.ID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);

        g_TextureCache[texture.path] = texture.ID;
        return true;
    }
}

void Mesh::ClearTextureCache()
{
    for (const auto &entry : g_TextureCache)
    {
        if (entry.second != 0)
        {
            glDeleteTextures(1, &entry.second);
        }
    }
    g_TextureCache.clear();
    g_FailedTextureLoads.clear();
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures, std::string name, bool deferSetup)
    : vertices(std::move(vertices)), indices(std::move(indices)), textures(std::move(textures)), name(std::move(name))
{
    if (!deferSetup)
    {
        setupMesh();
        m_GLObjectsInitialized = true;
    }
}

void Mesh::InitializeGLObjects()
{
    if (!m_GLObjectsInitialized)
    {
        setupMesh();
        m_GLObjectsInitialized = true;
    }
}

void Mesh::setupMesh()
{
    vao.Bind();

    // Create VBO & EBO
    vbo = new VBO((GLfloat *)vertices.data(), vertices.size() * sizeof(Vertex));
    ebo = new EBO(indices.data(), indices.size() * sizeof(GLuint));

    // Vertex attributes
    vao.LinkAttrib(*vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void *)0);                           // Position
    vao.LinkAttrib(*vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void *)offsetof(Vertex, Normal));    // Normal
    vao.LinkAttrib(*vbo, 2, 2, GL_FLOAT, sizeof(Vertex), (void *)offsetof(Vertex, TexCoords)); // TexCoords

    // Bone data
    vao.LinkAttribI(*vbo, 3, 4, GL_INT, sizeof(Vertex), (void *)offsetof(Vertex, BoneIDs));      // Bone IDs (integer)
    vao.LinkAttrib(*vbo, 4, 4, GL_FLOAT, sizeof(Vertex), (void *)offsetof(Vertex, BoneWeights)); // Bone Weights

    vao.Unbind();
    vbo->Unbind();
    ebo->Unbind();
}

void Mesh::Draw(Shader &shader, const MeshMaterialOverride *overrideMaterial)
{
    // Lazily initialize GL objects if not done yet
    if (!m_GLObjectsInitialized)
    {
        InitializeGLObjects();
    }
    shader.use();

    // Create a 1x1 white fallback texture once and use it whenever no valid GPU textures exist.
    static GLuint defaultWhiteTex = 0;
    if (defaultWhiteTex == 0)
    {
        unsigned char whitePixel[3] = {255, 255, 255};
        glGenTextures(1, &defaultWhiteTex);
        glBindTexture(GL_TEXTURE_2D, defaultWhiteTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    const MeshMaterialOverride *activeMaterial = overrideMaterial;
    MeshMaterialOverride fallbackMaterial;
    if (!activeMaterial)
    {
        activeMaterial = &fallbackMaterial;
    }

    bool forceColorOnly = activeMaterial->mode == MeshMaterialOverride::Mode::Color;
    bool hasBoundValidTexture = false;
    if (!forceColorOnly)
    {
        unsigned int boundTextureUnit = 0;

        Texture customTexture;
        if (activeMaterial->mode == MeshMaterialOverride::Mode::Texture && !activeMaterial->texturePath.empty())
        {
            customTexture.path = activeMaterial->texturePath;
            customTexture.type = GL_TEXTURE_2D;
            if (UploadTextureIfNeeded(customTexture))
            {
                glActiveTexture(GL_TEXTURE0 + boundTextureUnit);
                glBindTexture(GL_TEXTURE_2D, customTexture.ID);
                shader.setInt("uDiffuseMap", boundTextureUnit);
                shader.setBool("uUseTexture", true);
                shader.setVec3("uObjectColor", activeMaterial->color);
                hasBoundValidTexture = true;
            }
        }

        if (!hasBoundValidTexture && !textures.empty())
        {
            for (unsigned int i = 0; i < textures.size(); i++)
            {
                if (!UploadTextureIfNeeded(textures[i]))
                    continue;

                glActiveTexture(GL_TEXTURE0 + boundTextureUnit);
                glBindTexture(GL_TEXTURE_2D, textures[i].ID);

                // Engine shader currently consumes one diffuse map.
                shader.setInt("uDiffuseMap", boundTextureUnit);
                shader.setBool("uUseTexture", true);
                shader.setVec3("uObjectColor", activeMaterial->color);

                hasBoundValidTexture = true;
                break;
            }
        }
    }

    if (!hasBoundValidTexture)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, defaultWhiteTex);
        shader.setInt("uDiffuseMap", 0);
        shader.setBool("uUseTexture", false);
        shader.setVec3("uObjectColor", activeMaterial->color);
    }

    // Draw mesh
    vao.Bind();
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    vao.Unbind();

    glActiveTexture(GL_TEXTURE0); // Reset
}
