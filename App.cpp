#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "EBO.h"
#include "Shader.h"
#include "VAO.h"
#include "VBO.h"
#include "Window.h"

int main()
{
    Mara::Window game(900, 900, "Triangle Test");
    game.initialize();
    game.show();

    glfwSwapInterval(1);

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(game.getWindow(), &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f,
    };

    unsigned int indices[] = {0, 1, 2};

    VAO vao;
    vao.Bind();

    VBO vbo(vertices, sizeof(vertices));
    vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, 3 * sizeof(float), (void *)0);

    EBO ebo(indices, sizeof(indices));

    vao.Unbind();

    Shader shader("shaders/triangle.vert", "shaders/triangle.frag");

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    std::cout << "Rendering triangle. Close the window to exit.\n";

    while (!glfwWindowShouldClose(game.getWindow()))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        vao.Bind();
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);

        glfwSwapBuffers(game.getWindow());
        game.pollEvents();
    }

    ebo.Delete();
    vbo.Delete();
    vao.Delete();

    return 0;
}
