

#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Mara
{
    class Camera
    {
    public:
        void update(float deltaTime, bool viewportHovered, bool captureKeyboard);
        void reset();

        glm::mat4 viewMatrix() const;
        glm::mat4 projectionMatrix(int width, int height) const;
        glm::vec3 position() const;
        glm::vec3 forward() const;
        bool isFreeMode() const { return true; }

    private:
        float m_Yaw = 90.0f;
        float m_Pitch = 3.0f;
        glm::vec3 m_Position{3.8f, 0.45f, 0.0f};
    };
}