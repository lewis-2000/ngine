#include "EBO.h"

// Constructor: generate and bind an Element Buffer Object with index data
EBO::EBO(GLuint *indices, GLsizeiptr size)
{
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
}

// Bind the EBO
void EBO::Bind()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
}

// Unbind the EBO
void EBO::Unbind()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// Delete the EBO
void EBO::Delete()
{
    glDeleteBuffers(1, &ID);
}
