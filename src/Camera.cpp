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
        const bool freeTogglePressed = MaraGl::Input::IsKeyPressed(GLFW_KEY_F);

        if (freeTogglePressed && !m_FreeToggleLatch)
        {
            m_FreeMode = !m_FreeMode;
            if (m_FreeMode)
                m_FreePosition = position();
            else
                m_Target = m_FreePosition + forward() * m_Distance;
        }
        m_FreeToggleLatch = freeTogglePressed;

        if (m_FreeMode)
        {
            if (viewportHovered && rightMouseDown)
            {
                m_Yaw -= MaraGl::Input::GetMouseDeltaX() * 0.3f;
                m_Pitch = glm::clamp(
                    m_Pitch + MaraGl::Input::GetMouseDeltaY() * 0.3f,
                    -89.0f,
                    89.0f);
            }

            const glm::vec3 freeForward = forward();
            const glm::vec3 freeRight = glm::normalize(
                glm::cross(freeForward, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 movement(0.0f);
            if (!captureKeyboard)
            {
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_W))
                    movement += freeForward;
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_S))
                    movement -= freeForward;
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_D))
                    movement += freeRight;
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_A))
                    movement -= freeRight;
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_E))
                    movement.y += 1.0f;
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_Q))
                    movement.y -= 1.0f;
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_Z))
                    movement += freeForward;
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_X))
                    movement -= freeForward;
            }

            const float wheelDelta = viewportHovered
                                         ? MaraGl::Input::GetMouseWheelDelta()
                                         : 0.0f;
            m_FreePosition += freeForward * wheelDelta * 0.6f;
            if (glm::length(movement) > 0.0f)
                m_FreePosition += glm::normalize(movement) * deltaTime * 4.0f;
            if (!captureKeyboard)
            {
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_EQUAL))
                    m_FreePosition += freeForward * deltaTime * 5.0f;
                if (MaraGl::Input::IsKeyPressed(GLFW_KEY_MINUS))
                    m_FreePosition -= freeForward * deltaTime * 5.0f;
            }
            return;
        }

        if (viewportHovered && rightMouseDown)
        {
            m_Yaw -= MaraGl::Input::GetMouseDeltaX() * 0.3f;
            m_Pitch = glm::clamp(
                m_Pitch + MaraGl::Input::GetMouseDeltaY() * 0.3f,
                -80.0f,
                80.0f);
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
            const float panScale = m_Distance * 0.0025f;
            m_Target +=
                (-right * MaraGl::Input::GetMouseDeltaX() +
                 up * MaraGl::Input::GetMouseDeltaY()) *
                panScale;
        }

        const float wheelDelta = MaraGl::Input::GetMouseWheelDelta();
        if (viewportHovered)
            m_Distance -= wheelDelta * 0.6f;

        if (captureKeyboard)
        {
            m_Distance = glm::clamp(m_Distance, 2.5f, 20.0f);
            return;
        }

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

        m_Distance = glm::clamp(m_Distance, 2.5f, 20.0f);
    }

    void Camera::reset()
    {
        m_Distance = 5.5f;
        m_Yaw = 90.0f;
        m_Pitch = 3.0f;
        m_Target = glm::vec3(0.0f, 0.25f, 0.0f);
        m_FreePosition = position();
        m_FreeMode = false;
        m_FreeToggleLatch = false;
    }

    glm::vec3 Camera::position() const
    {
        if (m_FreeMode)
            return m_FreePosition;

        return m_Target + glm::vec3(
                              m_Distance * std::cos(glm::radians(m_Pitch)) * std::sin(glm::radians(m_Yaw)),
                              m_Distance * std::sin(glm::radians(m_Pitch)),
                              m_Distance * std::cos(glm::radians(m_Pitch)) * std::cos(glm::radians(m_Yaw)));
    }

    glm::vec3 Camera::forward() const
    {
        const float yaw = glm::radians(m_Yaw);
        const float pitch = glm::radians(m_Pitch);
        return glm::normalize(glm::vec3(
            -std::sin(yaw) * std::cos(pitch),
            -std::sin(pitch),
            -std::cos(yaw) * std::cos(pitch)));
    }

    glm::mat4 Camera::viewMatrix() const
    {
        const glm::vec3 cameraTarget =
            m_FreeMode ? position() + forward() : m_Target;
        return glm::lookAt(position(), cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 Camera::projectionMatrix(int width, int height) const
    {
        if (width <= 0 || height <= 0)
            return glm::mat4(1.0f);

        return glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(width) / static_cast<float>(height),
            0.1f,
            100.0f);
    }
}
