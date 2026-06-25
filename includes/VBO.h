#pragma once

#include <glad/glad.h>

class VBO
{
public:
    // Reference ID of the Vertex Buffer Object
    GLuint ID;
    // Constructor that generates a Vertex Buffer Object and links it to vertices
    VBO(GLfloat *vertices, GLsizeiptr size);

    void LinkAttrib(VBO &vbo, GLuint layout, GLint numComponents, GLenum type, GLsizei stride, void *offset);

    // Binds the VBO
    void Bind();
    // Unbinds the VBO
    void Unbind();
    // Deletes the VBO
    void Delete();
};
