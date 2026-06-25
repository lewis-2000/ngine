#include "VAO.h"

VAO::VAO()
{
    // Defer OpenGL object creation to first bind on the render thread.
    ID = 0;
}

void VAO::LinkAttrib(VBO &vbo, GLuint layout, GLint numComponents, GLenum type, GLsizei stride, void *offset)
{
    vbo.Bind();
    glVertexAttribPointer(
        layout,
        numComponents,
        type,
        GL_FALSE,
        stride,
        offset);
    glEnableVertexAttribArray(layout);
    vbo.Unbind();
}

void VAO::LinkAttribI(VBO &vbo, GLuint layout, GLint numComponents, GLenum type, GLsizei stride, void *offset)
{
    vbo.Bind();
    glVertexAttribIPointer(
        layout,
        numComponents,
        type,
        stride,
        offset);
    glEnableVertexAttribArray(layout);
    vbo.Unbind();
}

void VAO::Bind()
{
    if (ID == 0)
    {
        glGenVertexArrays(1, &ID);
    }
    glBindVertexArray(ID);
}

void VAO::Unbind()
{
    glBindVertexArray(0);
}

void VAO::Delete()
{
    if (ID != 0)
    {
        glDeleteVertexArrays(1, &ID);
        ID = 0;
    }
}
