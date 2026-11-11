#include "Camera.h"

#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Input.h"

namespace Mara
{
    namespace
    {
        constexpr float kCameraMoveSpeed = 3.5f;
        constexpr float kCameraFastMoveMultiplier = 3.0f;
        constexpr float kMouseLookSensitivity = 0.3f;
    }

    void Camera::update(float deltaTime, bool viewportHovered, bool captureKeyboard)
    {
        const bool rightMouseDown =
            MaraGl::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
        const bool middleMouseDown =
            MaraGl::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);

        if (viewportHovered && rightMouseDown)
        {
            m_Yaw -= MaraGl::Input::GetMouseDeltaX() * kMouseLookSensitivity;
            m_Pitch = glm::clamp(
                m_Pitch + MaraGl::Input::GetMouseDeltaY() * kMouseLookSensitivity,
                -89.0f,
                89.0f);
        }

        const glm::vec3 forward = this->forward();
        const glm::vec3 right = glm::normalize(glm::cross(
            forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up(0.0f, 1.0f, 0.0f);

        if (viewportHovered && middleMouseDown)
        {
            const float panScale = kCameraMoveSpeed * deltaTime;
            m_Position +=
                (-right * MaraGl::Input::GetMouseDeltaX() +
                 up * MaraGl::Input::GetMouseDeltaY()) *
                panScale;
        }

        if (captureKeyboard && !viewportHovered)
            return;

        if (!viewportHovered)
            return;

        const float wheelDelta = MaraGl::Input::GetMouseWheelDelta();
        m_Position += forward * wheelDelta * kCameraMoveSpeed * 0.35f;

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
            movement += up;
        if (MaraGl::Input::IsKeyPressed(GLFW_KEY_Q))
            movement -= up;

        if (glm::length(movement) > 0.0f)
        {
            const float speed = MaraGl::Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT)
                                    ? kCameraMoveSpeed * kCameraFastMoveMultiplier
                                    : kCameraMoveSpeed;
            m_Position += glm::normalize(movement) * speed * deltaTime;
        }
    }

    void Camera::reset()
    {
        m_Yaw = 90.0f;
        m_Pitch = 3.0f;
        m_Position = glm::vec3(3.8f, 0.45f, 0.0f);
    }

    glm::vec3 Camera::position() const
    {
        return m_Position;
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
        return glm::lookAt(position(), position() + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
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
