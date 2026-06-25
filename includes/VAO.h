#pragma once

#include <glad/glad.h>
#include "VBO.h"

class VAO
{
public:
    GLuint ID;
    VAO();

    // Flexible attribute linker
    void LinkAttrib(
        VBO &vbo,
        GLuint layout,       // layout index (shader)
        GLint numComponents, // e.g. 3 for vec3, 2 for vec2
        GLenum type,         // e.g. GL_FLOAT
        GLsizei stride,      // size of entire vertex (in bytes)
        void *offset         // offset in vertex struct
    );

    // Integer attribute linker (e.g. ivec4 bone IDs)
    void LinkAttribI(
        VBO &vbo,
        GLuint layout,
        GLint numComponents,
        GLenum type,
        GLsizei stride,
        void *offset);

    void Bind();
    void Unbind();
    void Delete();
};
