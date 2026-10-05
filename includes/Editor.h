#pragma once

#include <string>
#include <memory>

#include "Framebuffer.h"
#include "CameraCapture.h"
#include "PoseFrame.h"
#include "PoseEstimator.h"
#include "Robot.h"
#include "RosTelemetryClient.h"
#include <glm/mat4x4.hpp>

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
        const std::string &selectedLink() const { return m_SelectedLink; }
        bool showGrid() const { return m_ShowGrid; }
        bool showSensors() const { return m_ShowSensors; }
        bool simulationRunning() const { return m_SimulationRunning; }
        bool motionStreamingActive() const { return m_SimulationRunning; }
        float animationTime() const { return m_AnimationTime; }
        float animationDuration() const { return m_AnimationDuration; }
        int sceneWidth() const { return m_SceneFramebuffer.getWidth(); }
        int sceneHeight() const { return m_SceneFramebuffer.getHeight(); }
        void setGizmoMatrices(
            const glm::mat4 &view,
            const glm::mat4 &projection,
            const glm::mat4 &robotTransform);

        void shutdown();

    private:
        void setupDockspace();

        void drawToolbar();
        void drawScene();
        void drawViewport();
        void drawInspector();
        void drawMotion();
        void drawConsole();
        void drawPosePanel();
        void drawTelemetry();
        void applyTelemetryToRobot();
        void setSimulationTime(float time);
        void resetSimulation();

    private:
        bool m_Initialized = false;
        bool m_SimulationRunning = false;
        bool m_ShowGrid = true;
        bool m_ShowSensors = false;
        bool m_ShowConsole = false;
        bool m_ShowPosePanel = false;
        bool m_ShowTelemetry = true;
        bool m_ApplyTelemetryToRobot = false;
        RosTelemetryClient m_TelemetryClient;
        CameraCapture m_CameraCapture;
        std::unique_ptr<PoseEstimator> m_PoseEstimator;
        bool m_AnimateSelectedOnly = false;
        bool m_ViewportHovered = false;
        bool m_CameraResetRequested = false;
        Robot *m_Robot = nullptr;
        std::string m_SelectedLink;
        float m_AnimationTime = 0.0f;
        float m_AnimationDuration = 10.0f;
        bool m_LoopAnimation = true;
        PoseFrame m_PoseFrame;

        float m_Position[3] = {0.0f, 0.0f, 0.0f};
        float m_Rotation[3] = {0.0f, 0.0f, 0.0f};
        float m_Scale[3] = {1.0f, 1.0f, 1.0f};

    private:
        Framebuffer m_SceneFramebuffer;
        int m_RequestedSceneWidth = 0;
        int m_RequestedSceneHeight = 0;
        glm::mat4 m_GizmoView{1.0f};
        glm::mat4 m_GizmoProjection{1.0f};
        glm::mat4 m_RobotTransform{1.0f};
    };

}