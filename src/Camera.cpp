#include "Camera.h"

#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Input.h"

namespace Mara
{
    void Camera::update(float deltaTime, bool viewportHovered, bool captureKeyboard)
    {
        const bool rightMouseDown = MaraGl::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
        const bool middleMouseDown = MaraGl::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);

        if (viewportHovered && rightMouseDown)
        {
            m_Yaw -= MaraGl::Input::GetMouseDeltaX() * 0.25f;
            m_Pitch += MaraGl::Input::GetMouseDeltaY() * 0.25f;
            m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
        }

        const glm::vec3 forward = glm::normalize(glm::vec3(
            -std::sin(glm::radians(m_Yaw)),
            0.0f,
            -std::cos(glm::radians(m_Yaw))));
        const glm::vec3 right = glm::normalize(glm::cross(
            forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

        if (viewportHovered && middleMouseDown)
        {
            m_Target +=
                (-right * MaraGl::Input::GetMouseDeltaX() +
                 up * MaraGl::Input::GetMouseDeltaY()) *
                0.01f;
        }

        const float wheelDelta = MaraGl::Input::GetMouseWheelDelta();
        if (viewportHovered)
            m_Distance -= wheelDelta * 0.75f;

        if (captureKeyboard)
            return;

        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_EQUAL) ||
            MaraGl::Input::IsKeyPressed(GLFW_KEY_KP_ADD))
            m_Distance -= deltaTime * 5.0f;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_MINUS) ||
            MaraGl::Input::IsKeyPressed(GLFW_KEY_KP_SUBTRACT))
            m_Distance += deltaTime * 5.0f;

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
            m_Target += glm::normalize(movement) * (deltaTime * 3.0f);

        m_Distance = glm::clamp(m_Distance, 1.0f, 50.0f);
    }

    void Camera::reset()
    {
        m_Distance = 8.0f;
        m_Yaw = 0.0f;
        m_Pitch = 10.0f;
        m_Target = glm::vec3(0.0f);
    }

    glm::vec3 Camera::position() const
    {
        const float yaw = glm::radians(m_Yaw);
        const float pitch = glm::radians(m_Pitch);
        return m_Target + glm::vec3(
                              m_Distance * std::cos(pitch) * std::sin(yaw),
                              m_Distance * std::sin(pitch),
                              m_Distance * std::cos(pitch) * std::cos(yaw));
    }

    glm::mat4 Camera::viewMatrix() const
    {
        return glm::lookAt(position(), m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 Camera::projectionMatrix(int width, int height) const
    {
        return glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(width) / static_cast<float>(height),
            0.1f,
            100.0f);
    }
}
