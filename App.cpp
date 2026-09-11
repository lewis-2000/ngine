#include "App.h"

#include <iostream>
#include <memory>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "EBO.h"
#include "Shader.h"
#include "VAO.h"
#include "VBO.h"

namespace Mara
{
    App::App() = default;

    App::~App()
    {
        shutdown();
    }

    void App::initialize()
    {
        if (m_Window)
            return;

        m_Window = std::make_unique<Window>(900, 900, "Mara Engine");
        m_Window->initialize();
        m_Window->show();

        glfwSwapInterval(1);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(m_Window->getWindow(), &framebufferWidth, &framebufferHeight);
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        GLfloat vertices[] = {
            -0.5f,
            -0.5f,
            0.0f,
            0.5f,
            -0.5f,
            0.0f,
            0.0f,
            0.5f,
            0.0f,
        };
        GLuint indices[] = {0, 1, 2};

        m_VAO = std::make_unique<VAO>();
        m_VAO->Bind();

        m_VBO = std::make_unique<VBO>(vertices, sizeof(vertices));
        m_VAO->LinkAttrib(*m_VBO, 0, 3, GL_FLOAT, 3 * sizeof(float), nullptr);

        m_EBO = std::make_unique<EBO>(indices, sizeof(indices));
        m_VAO->Unbind();

        m_Shader = std::make_unique<Shader>("shaders/triangle.vert", "shaders/triangle.frag");
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }

    void App::run()
    {
        if (!m_Window)
            initialize();

        std::cout << "Rendering triangle. Close the window to exit.\n";

        while (!glfwWindowShouldClose(m_Window->getWindow()))
        {
            renderFrame();
            glfwSwapBuffers(m_Window->getWindow());
            m_Window->pollEvents();
        }
    }

    void App::renderFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT);
        m_Shader->use();
        m_VAO->Bind();
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
    }

    void App::shutdown()
    {
        if (!m_Window)
            return;

        if (m_EBO)
            m_EBO->Delete();
        if (m_VBO)
            m_VBO->Delete();
        if (m_VAO)
            m_VAO->Delete();

        m_Shader.reset();
        m_EBO.reset();
        m_VBO.reset();
        m_VAO.reset();
        m_Window.reset();
    }
}
