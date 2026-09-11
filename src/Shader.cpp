#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp> // For converting glm::mat4 to a float array

// Constructor: loads, compiles, and links vertex and fragment shaders
Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath)
{
    // Load vertex and fragment shader source code from file
    std::string vertexCode = loadShaderSource(vertexPath);
    std::string fragmentCode = loadShaderSource(fragmentPath);

    // Compile shaders
    unsigned int vertexShader = compileShader(vertexCode, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(fragmentCode, GL_FRAGMENT_SHADER);

    // Link shaders into a shader program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkCompileErrors(shaderProgram, "PROGRAM");
    ID = shaderProgram;

    // Shaders are now linked into the program, so they can be deleted
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

// Activate the shader program
void Shader::use() const
{
    glUseProgram(shaderProgram);
}

// Utility to set a bool uniform
void Shader::setBool(const std::string &name, bool value) const
{
    glUniform1i(glGetUniformLocation(shaderProgram, name.c_str()), (int)value);
}

// Utility to set an int uniform
void Shader::setInt(const std::string &name, int value) const
{
    glUniform1i(glGetUniformLocation(shaderProgram, name.c_str()), value);
}

// Utility to set a float uniform
void Shader::setFloat(const std::string &name, float value) const
{
    glUniform1f(glGetUniformLocation(shaderProgram, name.c_str()), value);
}

void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const
{
    // Get the location of the uniform in the shader
    unsigned int location = glGetUniformLocation(shaderProgram, name.c_str());

    // Send the matrix to the shader (make sure your shader is active)
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setMat4Array(const std::string &name, const std::vector<glm::mat4> &matrices) const
{
    if (matrices.empty())
        return;

    // Resolve array base location; some drivers expect "name[0]" for uniform arrays.
    GLint location = glGetUniformLocation(shaderProgram, name.c_str());
    if (location == -1)
    {
        std::string indexedName = name + "[0]";
        location = glGetUniformLocation(shaderProgram, indexedName.c_str());
    }
    if (location == -1)
        return;

    // Send the matrix array to the shader
    glUniformMatrix4fv(location, matrices.size(), GL_FALSE, glm::value_ptr(matrices[0]));
}

void Shader::setVec3(const std::string &name, const glm::vec3 &value) const
{
    glUniform3fv(glGetUniformLocation(shaderProgram, name.c_str()), 1, &value[0]); // use shaderProgram instead of ID
}

void Shader::setVec3(const std::string &name, float x, float y, float z) const
{
    glUniform3f(glGetUniformLocation(shaderProgram, name.c_str()), x, y, z); // use shaderProgram instead of ID
}

// Destructor to clean up the shader program
Shader::~Shader()
{
    glDeleteProgram(shaderProgram);
}

// Load shader source code from file
std::string Shader::loadShaderSource(const std::string &path) const
{
    std::ifstream shaderFile;
    std::stringstream shaderStream;

    // Open the shader file and read its contents
    shaderFile.open(path);
    if (!shaderFile.is_open())
    {
        std::cerr << "Failed to open shader file: " << path << std::endl;
        return "";
    }
    shaderStream << shaderFile.rdbuf();
    shaderFile.close();

    return shaderStream.str();
}

// Compile shader and return the shader ID
unsigned int Shader::compileShader(const std::string &source, GLenum shaderType) const
{
    const char *shaderCode = source.c_str();
    unsigned int shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderCode, nullptr);
    glCompileShader(shader);
    checkCompileErrors(shader, shaderType == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT");
    return shader;
}

// Check and print any shader compilation or linking errors
void Shader::checkCompileErrors(unsigned int shader, const std::string &type) const
{
    int success;
    char infoLog[1024];

    if (type == "PROGRAM")
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            // std::cerr << "ERROR::PROGRAM_LINKING_ERROR: " << infoLog << std::endl;
            Logger::runtimeError("Shader program linking error: " + std::string(infoLog));
        }
    }
    else
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            // std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
            //   << infoLog << std::endl;
            Logger::runtimeError("Shader compilation error (" + type + "): " + std::string(infoLog));
        }
    }
}