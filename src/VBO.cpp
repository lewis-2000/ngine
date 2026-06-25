#include "VBO.h"

// Constructor: generate and bind a Vertex Buffer Object with vertex data
VBO::VBO(GLfloat *vertices, GLsizeiptr size)
{
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ARRAY_BUFFER, ID);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

// Bind the VBO
void VBO::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, ID);
}

// Unbind the VBO
void VBO::Unbind()
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Delete the VBO
void VBO::Delete()
{
    glDeleteBuffers(1, &ID);
}
