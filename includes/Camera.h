

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

    private:
        float m_Distance = 8.0f;
        float m_Yaw = 0.0f;
        float m_Pitch = 10.0f;
        glm::vec3 m_Target{0.0f};
    };
}