#pragma once

#include <glad/glad.h>
#include <string>

class Shader;

class Texture
{
public:
    GLuint ID;
    std::string typeName;
    std::string path;
    GLenum type;

    Texture() : ID(0), type(GL_TEXTURE_2D) {}

    Texture(const char *image,
            GLenum texType,
            GLenum format,
            GLenum pixelType,
            const std::string &texName);

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

    ~Texture();

    void texUnit(const Shader &shader, const char *uniform, GLuint unit);
    void Bind() const;
    void Unbind() const;
    void Delete();
};
