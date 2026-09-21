#pragma once

#include "Framebuffer.h"
#include "Robot.h"

struct GLFWwindow;

namespace Mara
{

    class Editor
    {
    public:
        Editor() = default;
        ~Editor();

        void initialize(GLFWwindow *window, Robot *robot = nullptr);

        void beginFrame();
        void update(float deltaTime);
        void draw();
        void endFrame();

        void beginSceneRender(int width, int height);
        void endSceneRender();
        bool isViewportHovered() const { return m_ViewportHovered; }
        bool consumeCameraResetRequest();

        void shutdown();

    private:
        void setupDockspace();

        void drawToolbar();
        void drawScene();
        void drawViewport();
        void drawInspector();
        void drawMotion();
        void drawConsole();
        void setSimulationTime(float time);

    private:
        bool m_Initialized = false;
        bool m_SimulationRunning = false;
        bool m_ShowGrid = true;
        bool m_ViewportHovered = false;
        bool m_CameraResetRequested = false;
        Robot *m_Robot = nullptr;
        float m_AnimationTime = 0.0f;
        float m_AnimationDuration = 10.0f;
        bool m_LoopAnimation = true;

        float m_Position[3] = {0.0f, 0.0f, 0.0f};
        float m_Rotation[3] = {0.0f, 0.0f, 0.0f};
        float m_Scale[3] = {1.0f, 1.0f, 1.0f};

    private:
        Framebuffer m_SceneFramebuffer;
    };

}