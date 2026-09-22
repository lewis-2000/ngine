#include "App.h"

#include <iostream>
#include <memory>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Input.h"

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

        m_Window = std::make_unique<Window>(1920, 1080, "NGine");
        m_Window->initialize();
        m_Window->show();
        MaraGl::Input::Init(m_Window->getWindow());

        glfwSwapInterval(1);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(m_Window->getWindow(), &framebufferWidth, &framebufferHeight);
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        m_Robot = std::make_unique<Robot>();
        if (!m_Robot->LoadFromUrdf("resources/models/robot/urdf/humanoid.urdf"))
            std::cerr << "Failed to load robot: " << m_Robot->lastError() << '\n';
        else
            std::cout << "Loaded URDF robot '" << m_Robot->name()
                      << "' with " << m_Robot->data().links.size()
                      << " links and " << m_Robot->data().joints.size()
                      << " joints.\n";
        m_Plane = std::make_unique<Plane>(20.0f);
        m_Editor = std::make_unique<Editor>();

        m_RobotEntity = m_Scene.createEntity("robot");
        m_Scene.transform(m_RobotEntity)->rotation.x = glm::radians(-90.0f);
        m_Scene.transform(m_RobotEntity)->scale = glm::vec3(m_ModelScale);
        m_GroundEntity = m_Scene.createEntity("ground");
        m_Scene.transform(m_GroundEntity)->position.y = -0.72f;

        // Initialize ImGui and the editor
        m_Editor->initialize(m_Window->getWindow(), m_Robot.get());
        m_Renderer = std::make_unique<Renderer>(
            *m_Window,
            *m_Editor,
            *m_Robot,
            *m_Plane);
        m_Renderer->initialize();

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    }

    void App::run()
    {
        if (!m_Window)
            initialize();

        std::cout << "Rendering model. Close the window to exit.\n";

        m_LastFrameTime = glfwGetTime();
        while (!glfwWindowShouldClose(m_Window->getWindow()))
        {
            m_Window->pollEvents();
            double currentTime = glfwGetTime();
            float deltaTime = static_cast<float>(currentTime - m_LastFrameTime);
            m_LastFrameTime = currentTime;
            MaraGl::Input::Update();
            if (m_Editor->consumeCameraResetRequest() ||
                MaraGl::Input::IsKeyPressed(GLFW_KEY_R))
            {
                resetCamera();
            }
            updateCamera(deltaTime);
            m_Editor->update(deltaTime);
            renderFrame();
            glfwSwapBuffers(m_Window->getWindow());
        }
    }

    void App::updateCamera(float deltaTime)
    {
        m_Camera.update(
            deltaTime,
            m_Editor->isViewportHovered(),
            ImGui::GetIO().WantCaptureKeyboard);
    }

    void App::resetCamera()
    {
        m_Camera.reset();
    }

    void App::renderFrame()
    {
        m_Renderer->render(m_Scene, m_RobotEntity, m_GroundEntity, m_Camera);

        m_Editor->beginFrame();
        m_Editor->draw();

        m_Editor->endFrame();
    }

    void App::shutdown()
    {
        if (!m_Window)
            return;

        m_Renderer.reset();
        m_Editor.reset();
        m_Plane.reset();
        m_Robot.reset();
        m_Window.reset();
    }
}
