#pragma once

#include <memory>

#include "Robot.h"
#include "Plane.h"
#include "Window.h"
#include "Editor.h"
#include "Scene.h"
#include "Camera.h"
#include "Renderer.h"

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
        std::unique_ptr<Robot> m_Robot;
        std::unique_ptr<Plane> m_Plane;
        std::unique_ptr<Editor> m_Editor;
        std::unique_ptr<Renderer> m_Renderer;
        Scene m_Scene;
        Entity m_RobotEntity = NullEntity;
        Entity m_GroundEntity = NullEntity;

        float m_ModelScale = 1.0f;
        Camera m_Camera;
        double m_LastFrameTime = 0.0;
        float m_MouseWheel = 0.0f;
    };
}
