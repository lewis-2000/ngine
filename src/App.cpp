#include "App.h"

#include <iostream>
#include <memory>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/gtc/type_ptr.hpp>

#include "Input.h"
#include "Shader.h"

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

        m_Window = std::make_unique<Window>(900, 900, "NGine");
        m_Window->initialize();
        m_Window->show();
        MaraGl::Input::Init(m_Window->getWindow());

        glfwSwapInterval(1);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(m_Window->getWindow(), &framebufferWidth, &framebufferHeight);
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        m_Shader = std::make_unique<Shader>("shaders/scene.vert", "shaders/scene.frag");
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

        // Initialize ImGui and the editor
        m_Editor->initialize(m_Window->getWindow(), m_Robot.get());

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
            renderFrame();
            glfwSwapBuffers(m_Window->getWindow());
            m_Window->pollEvents();
        }
    }

    void App::updateCamera(float deltaTime)
    {
        const bool viewportHovered = m_Editor->isViewportHovered();
        const bool rightMouseDown = MaraGl::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
        const bool middleMouseDown = MaraGl::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);

        if (viewportHovered && rightMouseDown)
        {
            m_CameraYaw -= MaraGl::Input::GetMouseDeltaX() * 0.25f;
            m_CameraPitch += MaraGl::Input::GetMouseDeltaY() * 0.25f;
            m_CameraPitch = glm::clamp(m_CameraPitch, -89.0f, 89.0f);
        }

        if (viewportHovered && middleMouseDown)
        {
            glm::vec3 forward = glm::normalize(-glm::vec3(
                std::sin(glm::radians(m_CameraYaw)),
                0.0f,
                std::cos(glm::radians(m_CameraYaw))));
            glm::vec3 right = glm::normalize(glm::cross(
                forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 up = glm::normalize(glm::cross(right, forward));
            m_CameraTarget +=
                (-right * MaraGl::Input::GetMouseDeltaX() +
                 up * MaraGl::Input::GetMouseDeltaY()) *
                0.01f;
        }

        if (viewportHovered)
            m_CameraDistance -= MaraGl::Input::GetMouseWheelDelta() * 0.75f;
        m_CameraDistance = glm::clamp(m_CameraDistance, 1.0f, 50.0f);

        if (ImGui::GetIO().WantCaptureKeyboard)
            return;

        glm::vec3 forward = glm::normalize(-glm::vec3(
            std::sin(glm::radians(m_CameraYaw)),
            0.0f,
            std::cos(glm::radians(m_CameraYaw))));
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 movement(0.0f);

        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_W))
            movement += forward;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_S))
            movement -= forward;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_D))
            movement += right;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_A))
            movement -= right;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_E))
            movement.y += 1.0f;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_Q))
            movement.y -= 1.0f;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_Z))
            movement.z += 1.0f;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_X))
            movement.z -= 1.0f;

        if (glm::length(movement) > 0.0f)
            m_CameraTarget += glm::normalize(movement) * (deltaTime * 3.0f);
    }

    void App::resetCamera()
    {
        m_CameraDistance = 8.0f;
        m_CameraYaw = 0.0f;
        m_CameraPitch = 10.0f;
        m_CameraTarget = glm::vec3(0.0f);
    }

    void App::renderFrame()
    {
        m_Editor->beginSceneRender(m_Window->getWidth(), m_Window->getHeight());

        m_Shader->use();
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));
        model = glm::rotate(
            model,
            glm::radians(-90.0f),
            glm::vec3(1.0f, 0.0f, 0.0f));

        float yaw = glm::radians(m_CameraYaw);
        float pitch = glm::radians(m_CameraPitch);
        glm::vec3 cameraPosition = {
            m_CameraDistance * std::cos(pitch) * std::sin(yaw),
            m_CameraDistance * std::sin(pitch),
            m_CameraDistance * std::cos(pitch) * std::cos(yaw)};
        glm::mat4 view = glm::lookAt(
            cameraPosition + m_CameraTarget,
            m_CameraTarget,
            glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(m_Window->getWidth()) / static_cast<float>(m_Window->getHeight()),
            0.1f,
            100.0f);

        m_Shader->setMat4("model", model);
        m_Shader->setMat4("view", view);
        m_Shader->setMat4("projection", projection);
        m_Shader->setVec3("viewPos", cameraPosition);
        m_Shader->setVec3("lightDirection", -1.0f, -1.0f, -1.0f);
        m_Shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        const glm::mat4 groundTransform = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(0.0f, -0.72f, 0.0f));
        m_Plane->Draw(*m_Shader, groundTransform);
        m_Robot->Draw(*m_Shader, model);

        m_Editor->endSceneRender();

        m_Editor->beginFrame();
        m_Editor->draw();

        m_Editor->endFrame();
    }

    void App::shutdown()
    {
        if (!m_Window)
            return;

        m_Editor.reset();
        m_Plane.reset();
        m_Robot.reset();
        m_Shader.reset();
        m_Window.reset();
    }
}
