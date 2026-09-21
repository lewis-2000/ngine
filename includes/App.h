#pragma once

#include <memory>
#include <glm/vec3.hpp>

#include "Robot.h"
#include "Plane.h"
#include "Window.h"
#include "Editor.h"
#include "Shader.h"
#include "Scene.h"

class Shader;

namespace Mara
{
    class App
    {
    public:
        App();
        ~App();

        App(const App &) = delete;
        App &operator=(const App &) = delete;

        void initialize();
        void run();

    private:
        void renderFrame();
        void updateCamera(float deltaTime);
        void resetCamera();
        void shutdown();

        std::unique_ptr<Window> m_Window;
        std::unique_ptr<Shader> m_Shader;
        std::unique_ptr<Robot> m_Robot;
        std::unique_ptr<Plane> m_Plane;
        std::unique_ptr<Editor> m_Editor;
        Scene m_Scene;
        Entity m_RobotEntity = NullEntity;
        Entity m_GroundEntity = NullEntity;

        float m_ModelScale = 1.0f;
        float m_CameraDistance = 8.0f;
        float m_CameraYaw = 0.0f;
        float m_CameraPitch = 10.0f;
        glm::vec3 m_CameraTarget = glm::vec3(0.0f);
        double m_LastFrameTime = 0.0;
        float m_MouseWheel = 0.0f;
    };
}
