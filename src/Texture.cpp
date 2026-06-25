#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"
#include "Shader.h"
#include <iostream>

Texture::Texture(const char *image, GLenum texType, GLenum format, GLenum pixelType, const std::string &texName)
    : type(texType), typeName(texName)
{
    glGenTextures(1, &ID);
    glBindTexture(texType, ID);

    glTexParameteri(texType, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(texType, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(texType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(texType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(image, &width, &height, &nrChannels, 0);

    if (!data)
    {
        std::cerr << "Failed to load texture: " << image << std::endl;
        glDeleteTextures(1, &ID);
        ID = 0;
        glBindTexture(texType, 0);
        return;
    }

    GLenum internalFormat;
    if (nrChannels == 1)
        internalFormat = GL_R8;
    else if (nrChannels == 3)
        internalFormat = GL_RGB8;
    else if (nrChannels == 4)
        internalFormat = GL_RGBA8;
    else
    {
        std::cerr << "Unsupported channel count: " << nrChannels << " in " << image << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &ID);
        ID = 0;
        glBindTexture(texType, 0);
        return;
    }

    glTexImage2D(texType, 0, internalFormat, width, height, 0, format, pixelType, data);
    glGenerateMipmap(texType);
    path = image;
    stbi_image_free(data);
    glBindTexture(texType, 0);

    std::cout << "Loading " << image << " ("
              << width << "x" << height << ", channels=" << nrChannels << ")\n";
}

Texture::~Texture()
{
    Delete();
}

void Texture::texUnit(const Shader &shader, const char *uniform, GLuint unit)
{
    shader.use();
    glActiveTexture(GL_TEXTURE0 + unit);
    Bind();
    glUniform1i(glGetUniformLocation(shader.ID, uniform), static_cast<GLint>(unit));
}

void Texture::Bind() const
{
    glBindTexture(type, ID);
}

void Texture::Unbind() const
{
    glBindTexture(type, 0);
}

void Texture::Delete()
{
    if (ID != 0)
    {
        glDeleteTextures(1, &ID);
        ID = 0;
    }
}
