#pragma once

// Any other includes can follow
#include <glad/glad.h>
#include <string>
#include <vector>
#include <glm/glm.hpp> // For glm types like mat4, vec3, etc.
#include "Logger.h"
// Include glad to load OpenGL functions

class Shader
{
public:
    unsigned int ID; // shader program ID

    // Constructor that loads and compiles shaders from given paths
    Shader(const std::string &vertexPath, const std::string &fragmentPath);

    // Activate (use) the shader program
    void use() const;

    // Utility functions for setting uniforms (e.g., for later use)
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;
    void setMat4Array(const std::string &name, const std::vector<glm::mat4> &matrices) const;
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;

    // Destructor to clean up the shader program
    ~Shader();

private:
    // The OpenGL shader program ID
    unsigned int shaderProgram;

    // Load shader from file
    std::string loadShaderSource(const std::string &path) const;

    // Compile a shader
    unsigned int compileShader(const std::string &source, GLenum shaderType) const;

    // Check for shader compilation errors
    void checkCompileErrors(unsigned int shader, const std::string &type) const;
};
