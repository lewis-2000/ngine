#pragma once

#include <string>
#include <memory>

#include "Framebuffer.h"
#include "Animation.h"
#include "CameraCapture.h"
#include "PoseFrame.h"
#include "PoseEstimator.h"
#include "Robot.h"
#include "RosTelemetryClient.h"
#include "OccupancyMap.h"
#include <glm/mat4x4.hpp>

struct GLFWwindow;
struct ImFont;

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
        void dispatchAnimationEvent(const AnimationEvent &event);
        bool dispatchAnimationIntent(
            const std::string &intent,
            int priority = 0,
            bool interruptCurrent = false);
        bool dispatchAnimationIntentJson(const std::string &json);
        const std::string &selectedLink() const { return m_SelectedLink; }
        bool showGrid() const { return m_ShowGrid; }
        bool showSensors() const { return m_ShowSensors; }
        bool simulationRunning() const { return m_SimulationRunning; }
        bool motionStreamingActive() const { return m_SimulationRunning; }
        bool mapModeActive() const { return m_ShowMap || m_SimulationWorkspace; }
        float animationTime() const { return m_AnimationTime; }
        float animationDuration() const { return m_AnimationDuration; }
        const std::string &currentAnimation() const
        {
            return m_AnimationMixer.currentAnimation();
        }
        bool animationSafetyAllowsMotion() const
        {
            return !m_AnimationSafety.emergencyStop &&
                   m_AnimationSafety.motorsEnabled;
        }
        const std::string &animationStatus() const { return m_AnimationStatus; }
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
        void drawMapPanel();
        void drawSimulationWorkspace();
        void updateSimulation(float deltaTime);
        void enterSimulationWorkspace();
        void resetTestEnvironment();
        void applyTelemetryToRobot();
        void updateRemoteCameraTexture();
        void refreshDemoAnimation();
        void setSimulationTime(float time);
        void resetSimulation();
        void captureAuthoringKeyframe();
        bool saveAuthoringAnimation();
        void previewAnimation(const AnimationClip &clip);

    private:
        ImFont *m_FontBody = nullptr;
        ImFont *m_FontHeading = nullptr;
        ImFont *m_FontMono = nullptr;

        bool m_Initialized = false;
        bool m_SimulationRunning = false;
        bool m_ShowGrid = true;
        bool m_ShowSensors = false;
        bool m_ShowConsole = false;
        bool m_ShowPosePanel = false;
        bool m_ShowAnimationPanel = true;
        bool m_ShowSensorOverlays = true;
        bool m_ShowTelemetry = true;
        bool m_ShowMap = false;
        bool m_SimulationWorkspace = false;
        bool m_ShowWorkspaceChooser = true;
        bool m_TestEnvironmentGenerated = false;
        bool m_MapTopView = true;
        char m_MapPath[260] = "maps/scene.map";
        bool m_ShowMotorTelemetryPanel = false;
        bool m_ApplyTelemetryToRobot = false;
        unsigned int m_RemoteCameraTexture = 0;
        unsigned int m_RemoteDepthTexture = 0;
        std::uint64_t m_RemoteCameraSequence = 0;
        std::uint64_t m_RemoteDepthSequence = 0;
        int m_RemoteCameraWidth = 0;
        int m_RemoteCameraHeight = 0;
        int m_RemoteDepthWidth = 0;
        int m_RemoteDepthHeight = 0;
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
        AnimationSafetyState m_AnimationSafety;
        PoseFrame m_PoseFrame;
        AnimationLibrary m_AnimationLibrary;
        AnimationEventMapper m_AnimationEventMapper;
        AnimationEventQueue m_AnimationEventQueue;
        AnimationSelector m_AnimationSelector;
        AnimationMixer m_AnimationMixer;
        std::string m_AnimationConfiguration;
        std::string m_AnimationStatus;
        AnimationClip m_AuthoringClip;
        std::string m_AuthoringName = "new_animation";
        float m_AuthoringDuration = 2.0f;
        float m_AuthoringTime = 0.0f;
        std::uint64_t m_LastTelemetrySequence = 0;
        std::uint64_t m_LastMappedDepthSequence = 0;
        bool m_TelemetryErrorLatched = false;
        OccupancyMap m_OccupancyMap;
        OccupancyMap m_SimulationMap;
        glm::vec3 m_SimulatedRobotPosition{-4.0f, 0.25f, -4.0f};
        float m_SimulatedRobotYaw = 0.0f;
        float m_SimulationSpeed = 2.0f;
        std::string m_SimulationStatus = "Ready";
        bool m_MapNavigationMode = true;
        float m_MapOrbitYaw = 0.65f;
        float m_MapOrbitPitch = 0.55f;
        float m_MapZoom = 1.0f;
        glm::vec2 m_MapPan{0.0f};
        int m_MapViewPreset = 0;

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