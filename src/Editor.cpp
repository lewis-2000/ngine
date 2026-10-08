#include "Editor.h"

#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

namespace Mara
{
    namespace
    {
        constexpr ImU32 kViewportCardBackground = IM_COL32(26, 31, 39, 242);
        constexpr ImU32 kViewportCardBorder = IM_COL32(105, 122, 143, 220);
        constexpr ImU32 kViewportImageBackground = IM_COL32(13, 17, 22, 255);
        constexpr ImU32 kViewportText = IM_COL32(232, 238, 246, 255);
        constexpr ImU32 kViewportMutedText = IM_COL32(165, 176, 190, 255);
        constexpr ImU32 kTextureTint = IM_COL32(255, 255, 255, 255);
        const ImVec4 kViewportPanelBackground(0.10f, 0.12f, 0.15f, 1.0f);
        const ImVec4 kViewportAccent(0.35f, 0.78f, 0.95f, 1.0f);
        const ImVec4 kHealthyStatus(0.25f, 0.70f, 0.38f, 1.0f);

        void drawPanelHeader(
            const char *icon,
            const char *title,
            const char *status = nullptr)
        {
            ImGui::TextColored(ImVec4(0.10f, 0.30f, 0.48f, 1.0f), "%s", icon);
            ImGui::SameLine();
            ImGui::TextUnformatted(title);
            if (status)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
                ImGui::TextDisabled("%s", status);
            }
            ImGui::Separator();
        }

        void drawStatusBadge(
            const char *label,
            const ImVec4 &color)
        {
            ImGui::TextColored(color, "%s", label);
        }
    }

    Editor::~Editor()
    {
        shutdown();
    }

    void Editor::initialize(GLFWwindow *window, Robot *robot)
    {
        if (m_Initialized)
            return;

        m_Robot = robot;

        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();

        // Enable ImGui docking
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ImGui::StyleColorsDark();

        ImGuiStyle &style = ImGui::GetStyle();

        // ------------------------------------------------------------
        // Geometry
        // ------------------------------------------------------------

        style.WindowRounding = 5.0f;
        style.ChildRounding = 3.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;

        style.WindowPadding = ImVec2(10.0f, 8.0f);
        style.FramePadding = ImVec2(7.0f, 4.0f);
        style.ItemSpacing = ImVec2(7.0f, 5.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);

        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;

        // ------------------------------------------------------------
        // Colors
        // ------------------------------------------------------------

        ImVec4 *colors = style.Colors;

        // Background hierarchy
        colors[ImGuiCol_WindowBg] =
            ImVec4(0.045f, 0.050f, 0.060f, 1.00f);

        colors[ImGuiCol_ChildBg] =
            ImVec4(0.060f, 0.067f, 0.080f, 1.00f);

        colors[ImGuiCol_PopupBg] =
            ImVec4(0.065f, 0.072f, 0.085f, 0.98f);

        colors[ImGuiCol_MenuBarBg] =
            ImVec4(0.035f, 0.040f, 0.050f, 1.00f);

        // Window title bars
        colors[ImGuiCol_TitleBg] =
            ImVec4(0.035f, 0.040f, 0.050f, 1.00f);

        colors[ImGuiCol_TitleBgActive] =
            ImVec4(0.070f, 0.100f, 0.140f, 1.00f);

        colors[ImGuiCol_TitleBgCollapsed] =
            ImVec4(0.035f, 0.040f, 0.050f, 1.00f);

        // Borders / separators
        colors[ImGuiCol_Border] =
            ImVec4(0.16f, 0.19f, 0.24f, 0.85f);

        colors[ImGuiCol_BorderShadow] =
            ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        colors[ImGuiCol_Separator] =
            ImVec4(0.14f, 0.17f, 0.21f, 0.75f);

        colors[ImGuiCol_SeparatorHovered] =
            ImVec4(0.20f, 0.42f, 0.60f, 0.75f);

        colors[ImGuiCol_SeparatorActive] =
            ImVec4(0.25f, 0.55f, 0.78f, 1.00f);

        // Text
        colors[ImGuiCol_Text] =
            ImVec4(0.88f, 0.90f, 0.94f, 1.00f);

        colors[ImGuiCol_TextDisabled] =
            ImVec4(0.45f, 0.49f, 0.56f, 1.00f);

        // Controls
        colors[ImGuiCol_FrameBg] =
            ImVec4(0.075f, 0.085f, 0.105f, 1.00f);

        colors[ImGuiCol_FrameBgHovered] =
            ImVec4(0.105f, 0.135f, 0.175f, 1.00f);

        colors[ImGuiCol_FrameBgActive] =
            ImVec4(0.12f, 0.20f, 0.28f, 1.00f);

        // Buttons
        colors[ImGuiCol_Button] =
            ImVec4(0.075f, 0.105f, 0.140f, 1.00f);

        colors[ImGuiCol_ButtonHovered] =
            ImVec4(0.11f, 0.18f, 0.25f, 1.00f);

        colors[ImGuiCol_ButtonActive] =
            ImVec4(0.14f, 0.25f, 0.34f, 1.00f);

        // Headers / tree nodes
        colors[ImGuiCol_Header] =
            ImVec4(0.075f, 0.12f, 0.17f, 1.00f);

        colors[ImGuiCol_HeaderHovered] =
            ImVec4(0.11f, 0.20f, 0.28f, 1.00f);

        colors[ImGuiCol_HeaderActive] =
            ImVec4(0.14f, 0.27f, 0.38f, 1.00f);

        // Selection / accent
        colors[ImGuiCol_CheckMark] =
            ImVec4(0.30f, 0.68f, 0.92f, 1.00f);

        colors[ImGuiCol_SliderGrab] =
            ImVec4(0.25f, 0.58f, 0.82f, 1.00f);

        colors[ImGuiCol_SliderGrabActive] =
            ImVec4(0.35f, 0.72f, 0.96f, 1.00f);

        // Tabs
        colors[ImGuiCol_Tab] =
            ImVec4(0.055f, 0.075f, 0.100f, 1.00f);

        colors[ImGuiCol_TabHovered] =
            ImVec4(0.11f, 0.20f, 0.28f, 1.00f);

        colors[ImGuiCol_TabSelected] =
            ImVec4(0.09f, 0.17f, 0.24f, 1.00f);

        colors[ImGuiCol_TabSelectedOverline] =
            ImVec4(0.25f, 0.62f, 0.88f, 1.00f);

        colors[ImGuiCol_TabDimmed] =
            ImVec4(0.045f, 0.060f, 0.080f, 1.00f);

        colors[ImGuiCol_TabDimmedSelected] =
            ImVec4(0.075f, 0.12f, 0.17f, 1.00f);

        // Docking
        colors[ImGuiCol_DockingPreview] =
            ImVec4(0.25f, 0.62f, 0.88f, 0.35f);

        colors[ImGuiCol_DockingEmptyBg] =
            ImVec4(0.030f, 0.035f, 0.045f, 1.00f);

        io.Fonts->AddFontFromFileTTF(
            "resources/fonts/IBMPlexSans-Variable.ttf",
            22.0f);

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        m_CameraCapture.open();
        m_PoseEstimator = createPoseEstimator();

        if (m_Robot && m_Robot->isLoaded())
            m_SelectedLink = m_Robot->rootLink();

        m_Initialized = true;
    }

    void Editor::update(float deltaTime)
    {
        m_TelemetryClient.update();
        if (m_TelemetryClient.hasData())
        {
            const RosTelemetrySnapshot &snapshot =
                m_TelemetryClient.snapshot();
            if (snapshot.sequence != m_LastTelemetrySequence)
            {
                m_LastTelemetrySequence = snapshot.sequence;
                bool hasMotorError = false;
                for (const MotorTelemetry &motor : snapshot.motors)
                    hasMotorError = hasMotorError || motor.error != 0;

                if (hasMotorError && !m_TelemetryErrorLatched)
                {
                    dispatchAnimationEvent(
                        {AnimationEventType::Error, 100, "telemetry"});
                    m_TelemetryErrorLatched = true;
                }
                else if (!hasMotorError)
                {
                    m_TelemetryErrorLatched = false;
                }
            }
        }
        updateRemoteCameraTexture();
        applyTelemetryToRobot();

        AnimationEvent event;
        if (m_AnimationEventQueue.tryPop(event))
        {
            const AnimationRequest request =
                m_AnimationEventMapper.toRequest(event);
            refreshDemoAnimation();
            const AnimationClip *selected =
                m_AnimationSelector.select(m_AnimationLibrary.catalog(), request);
            const bool canInterrupt =
                !m_AnimationMixer.hasClip() ||
                request.interruptCurrent ||
                request.priority > m_AnimationMixer.priority();
            const bool robotReady = m_Robot && m_Robot->isLoaded();
            const bool safetyAllows =
                selected && AnimationSafety::allows(*selected, m_AnimationSafety);
            if (robotReady && selected && canInterrupt && safetyAllows)
            {
                m_AnimationMixer.setClip(*selected, *m_Robot);
                m_AnimationTime = 0.0f;
                m_SimulationRunning = true;
                m_AnimationStatus = "Playing " + selected->name;
            }
            else if (!robotReady)
            {
                m_AnimationStatus = "Animation rejected: robot is not ready";
            }
            else if (!selected)
            {
                m_AnimationStatus =
                    "Animation rejected: no clip matches " + request.intent;
            }
            else if (!canInterrupt)
            {
                m_AnimationStatus =
                    "Animation rejected: active animation has higher priority";
            }
            else
            {
                m_AnimationStatus = "Animation rejected: " +
                                    std::string(AnimationSafety::rejectionReason(
                                        *selected,
                                        m_AnimationSafety));
            }
        }

        if (m_CameraCapture.isOpen())
            m_CameraCapture.update();

        m_PoseEstimator->update(
            deltaTime,
            m_CameraCapture.pixels().data(),
            m_CameraCapture.width(),
            m_CameraCapture.height());
        m_PoseFrame = m_PoseEstimator->frame();

        if (!m_Robot || !m_Robot->isLoaded() || !m_SimulationRunning)
            return;

        m_AnimationMixer.update(deltaTime, *m_Robot);
        m_AnimationTime = m_AnimationMixer.time();
        if (!m_AnimationMixer.hasClip())
        {
            m_SimulationRunning = false;
            m_AnimationStatus = "Animation playback complete";
        }
    }

    void Editor::applyTelemetryToRobot()
    {
        if (!m_ApplyTelemetryToRobot ||
            m_SimulationRunning ||
            !m_Robot ||
            !m_Robot->isLoaded() ||
            !m_TelemetryClient.connected() ||
            m_TelemetryClient.stale())
            return;

        const auto &motors = m_TelemetryClient.snapshot().motors;
        const auto positionFor = [&motors](int id) -> const MotorTelemetry *
        {
            const auto it = std::find_if(
                motors.begin(),
                motors.end(),
                [id](const MotorTelemetry &motor)
                { return motor.id == id; });
            return it == motors.end() ? nullptr : &*it;
        };

        const auto applyGroup = [&positionFor, this](
                                    const std::array<int, 7> &ids,
                                    const std::array<const char *, 7> &joints)
        {
            for (std::size_t index = 0; index < ids.size(); ++index)
            {
                const MotorTelemetry *motor = positionFor(ids[index]);
                if (motor && motor->error == 0)
                    m_Robot->setJointPosition(joints[index], motor->position);
            }
        };

        applyGroup(
            {11, 12, 13, 14, 15, 16, 17},
            {"left_joint1", "shoulder_roll_l_joint", "left_joint3",
             "elbow_l_joint", "left_joint5", "left_joint6", "left_joint7"});
        applyGroup(
            {21, 22, 23, 24, 25, 26, 27},
            {"right_joint1", "shoulder_roll_r_joint", "right_joint3",
             "elbow_r_joint", "right_joint5", "right_joint6", "right_joint7"});

        const auto applySix = [&positionFor, this](
                                  const std::array<int, 6> &ids,
                                  const std::array<const char *, 6> &joints)
        {
            for (std::size_t index = 0; index < ids.size(); ++index)
            {
                const MotorTelemetry *motor = positionFor(ids[index]);
                if (motor && motor->error == 0)
                    m_Robot->setJointPosition(joints[index], motor->position);
            }
        };

        applySix(
            {51, 52, 53, 54, 55, 56},
            {"hip_roll_l_joint", "hip_yaw_l_joint", "hip_pitch_l_joint",
             "knee_pitch_l_joint", "ankle_pitch_l_joint", "ankle_roll_l_joint"});
        applySix(
            {61, 62, 63, 64, 65, 66},
            {"hip_roll_r_joint", "hip_yaw_r_joint", "hip_pitch_r_joint",
             "knee_pitch_r_joint", "ankle_pitch_r_joint", "ankle_roll_r_joint"});

        if (const MotorTelemetry *motor = positionFor(31); motor && motor->error == 0)
            m_Robot->setJointPosition("waist_joint", motor->position);
    }

    void Editor::updateRemoteCameraTexture()
    {
        const auto &snapshot = m_TelemetryClient.snapshot();
        if (snapshot.cameraSequence == m_RemoteCameraSequence ||
            snapshot.cameraRgb.empty() ||
            snapshot.cameraWidth <= 0 ||
            snapshot.cameraHeight <= 0)
        {
            if (snapshot.depthSequence == m_RemoteDepthSequence)
                return;
        }

        if (!snapshot.cameraRgb.empty() &&
            snapshot.cameraSequence != m_RemoteCameraSequence &&
            snapshot.cameraWidth > 0 &&
            snapshot.cameraHeight > 0 &&
            snapshot.cameraRgb.size() ==
                static_cast<std::size_t>(snapshot.cameraWidth) *
                    static_cast<std::size_t>(snapshot.cameraHeight) * 3u)
        {
            if (m_RemoteCameraTexture == 0)
                glGenTextures(1, &m_RemoteCameraTexture);
            glBindTexture(GL_TEXTURE_2D, m_RemoteCameraTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLint unpackAlignment = 4;
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGB8,
                snapshot.cameraWidth,
                snapshot.cameraHeight,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                snapshot.cameraRgb.data());
            glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
            m_RemoteCameraSequence = snapshot.cameraSequence;
            m_RemoteCameraWidth = snapshot.cameraWidth;
            m_RemoteCameraHeight = snapshot.cameraHeight;
        }

        if (snapshot.depthSequence != m_RemoteDepthSequence &&
            !snapshot.depth.empty() &&
            snapshot.depthWidth > 0 &&
            snapshot.depthHeight > 0 &&
            snapshot.depth.size() ==
                static_cast<std::size_t>(snapshot.depthWidth) *
                    static_cast<std::size_t>(snapshot.depthHeight))
        {
            std::vector<std::uint8_t> depthRgb(snapshot.depth.size() * 3u);
            for (std::size_t index = 0; index < snapshot.depth.size(); ++index)
            {
                const std::uint16_t millimeters = snapshot.depth[index];
                const std::uint8_t intensity = static_cast<std::uint8_t>(
                    std::clamp(255 - (static_cast<int>(millimeters) * 255 / 5000), 0, 255));
                depthRgb[index * 3u] = intensity;
                depthRgb[index * 3u + 1u] = intensity;
                depthRgb[index * 3u + 2u] = intensity;
            }
            if (m_RemoteDepthTexture == 0)
                glGenTextures(1, &m_RemoteDepthTexture);
            glBindTexture(GL_TEXTURE_2D, m_RemoteDepthTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLint unpackAlignment = 4;
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGB8,
                snapshot.depthWidth,
                snapshot.depthHeight,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                depthRgb.data());
            glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
            m_RemoteDepthSequence = snapshot.depthSequence;
            m_RemoteDepthWidth = snapshot.depthWidth;
            m_RemoteDepthHeight = snapshot.depthHeight;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Editor::setGizmoMatrices(
        const glm::mat4 &view,
        const glm::mat4 &projection,
        const glm::mat4 &robotTransform)
    {
        m_GizmoView = view;
        m_GizmoProjection = projection;
        m_RobotTransform = robotTransform;
    }

    void Editor::setSimulationTime(float time)
    {
        if (!m_Robot || !m_Robot->isLoaded())
            return;

        refreshDemoAnimation();
        m_AnimationMixer.seek(time, *m_Robot);
        m_AnimationTime = m_AnimationMixer.time();
    }

    void Editor::refreshDemoAnimation()
    {
        if (!m_Robot || !m_Robot->isLoaded())
            return;

        const std::string configuration =
            (m_AnimateSelectedOnly ? "selected:" + m_SelectedLink : "all") +
            (m_LoopAnimation ? ":loop" : ":once");
        if (configuration == m_AnimationConfiguration)
            return;

        m_AnimationLibrary.rebuild(
            *m_Robot,
            m_AnimationDuration,
            m_LoopAnimation,
            m_AnimateSelectedOnly,
            m_SelectedLink);

        const AnimationRequest request{"demo_wave", 0};
        if (const AnimationClip *selected =
                m_AnimationSelector.select(m_AnimationLibrary.catalog(), request))
        {
            if (const AnimationClip *idle =
                    m_AnimationLibrary.catalog().find("idle"))
                m_AnimationMixer.setIdleClip(*idle);
            m_AnimationMixer.setClip(*selected, *m_Robot);
        }
        m_AnimationConfiguration = configuration;
    }

    void Editor::dispatchAnimationEvent(const AnimationEvent &event)
    {
        m_AnimationEventQueue.push(event);
    }

    bool Editor::dispatchAnimationIntent(
        const std::string &intent,
        int priority,
        bool interruptCurrent)
    {
        AnimationEventType type;
        if (intent == "startup")
            type = AnimationEventType::Startup;
        else if (intent == "idle")
            type = AnimationEventType::Idle;
        else if (intent == "greeting")
            type = AnimationEventType::Greeting;
        else if (intent == "alert")
            type = AnimationEventType::Alert;
        else if (intent == "error")
            type = AnimationEventType::Error;
        else
            return false;

        dispatchAnimationEvent(
            {type,
             interruptCurrent ? std::max(priority, 10) : priority,
             "external-intent"});
        return true;
    }

    bool Editor::dispatchAnimationIntentJson(const std::string &json)
    {
        AnimationRequest request;
        if (!m_AnimationEventMapper.parseIntentJson(json, request))
            return false;
        return dispatchAnimationIntent(
            request.intent,
            request.priority,
            request.interruptCurrent);
    }

    void Editor::resetSimulation()
    {
        m_SimulationRunning = false;
        m_AnimationTime = 0.0f;

        if (m_Robot && m_Robot->isLoaded())
            m_Robot->resetJointPositions();
    }

    void Editor::setupDockspace()
    {
        ImGuiID dockspaceID = ImGui::GetID("MainDockspace");

        ImGui::DockSpaceOverViewport(
            dockspaceID,
            ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_NoTabBar);

        // ------------------------------------------------------------
        // Only build the layout if this dockspace doesn't have a
        // layout yet.
        // ------------------------------------------------------------

        static bool layoutInitialized = false;

        if (layoutInitialized)
            return;

        layoutInitialized = true;

        // Remove any existing layout
        ImGui::DockBuilderRemoveNode(dockspaceID);

        // Create the main dockspace
        ImGui::DockBuilderAddNode(
            dockspaceID,
            ImGuiDockNodeFlags_DockSpace);

        ImGui::DockBuilderSetNodeSize(
            dockspaceID,
            ImGui::GetMainViewport()->Size);

        // ------------------------------------------------------------
        // Start with:
        //
        // ┌─────────────────────────────────────────┐
        // │                                         │
        // │              Main                       │
        // │                                         │
        // └─────────────────────────────────────────┘
        // ------------------------------------------------------------

        ImGuiID mainNode = dockspaceID;

        // ------------------------------------------------------------
        // Keep the World hierarchy compact and leave most space for the
        // viewport and selected-object inspection.
        // ------------------------------------------------------------

        ImGuiID leftNode;
        ImGuiID centerNode;

        ImGui::DockBuilderSplitNode(
            mainNode,
            ImGuiDir_Left,
            0.18f,
            &leftNode,
            &centerNode);

        ImGuiID rightNode;
        ImGuiID viewportNode;

        ImGui::DockBuilderSplitNode(
            centerNode,
            ImGuiDir_Right,
            0.28f,
            &rightNode,
            &viewportNode);

        ImGuiID bottomNode;
        ImGuiID finalViewportNode;

        ImGui::DockBuilderSplitNode(
            viewportNode,
            ImGuiDir_Down,
            0.25f,
            &bottomNode,
            &finalViewportNode);

        ImGui::DockBuilderDockWindow(
            "World",
            leftNode);

        ImGui::DockBuilderDockWindow(
            "Inspector",
            rightNode);

        ImGui::DockBuilderDockWindow(
            "Viewport",
            finalViewportNode);

        ImGui::DockBuilderDockWindow(
            "Console",
            bottomNode);

        ImGui::DockBuilderDockWindow(
            "Robot telemetry",
            bottomNode);

        // Finish building the layout
        ImGui::DockBuilderFinish(dockspaceID);
    }

    void Editor::beginFrame()
    {
        if (!m_Initialized)
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void Editor::draw()
    {
        if (!m_Initialized)
            return;

        setupDockspace();

        drawToolbar();
        drawScene();
        drawViewport();
        drawInspector();
        drawMotion();
        drawConsole();
        drawPosePanel();
        drawTelemetry();
    }

    void Editor::beginSceneRender(int width, int height)
    {
        if (!m_Initialized)
            return;

        const int sceneWidth = m_RequestedSceneWidth > 0 ? m_RequestedSceneWidth : width;
        const int sceneHeight = m_RequestedSceneHeight > 0 ? m_RequestedSceneHeight : height;
        if (m_SceneFramebuffer.getWidth() == 0 || m_SceneFramebuffer.getHeight() == 0)
            m_SceneFramebuffer.initialize(sceneWidth, sceneHeight);
        else
            m_SceneFramebuffer.resize(sceneWidth, sceneHeight);
        m_SceneFramebuffer.bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void Editor::endSceneRender()
    {
        if (m_Initialized)
            m_SceneFramebuffer.unbind();
    }

    bool Editor::consumeCameraResetRequest()
    {
        const bool requested = m_CameraResetRequested;
        m_CameraResetRequested = false;
        return requested;
    }

    void Editor::drawToolbar()
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        ImGui::TextColored(ImVec4(0.08f, 0.32f, 0.58f, 1.0f), "NGINE");
        ImGui::SameLine();
        ImGui::TextDisabled("ROBOTICS STUDIO");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New world");
            ImGui::MenuItem("Open world...");
            ImGui::MenuItem("Save world", "Ctrl+S");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Y");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Grid", nullptr, &m_ShowGrid);
            ImGui::MenuItem("Sensor visualization", nullptr, &m_ShowSensors);
            ImGui::MenuItem("Console", nullptr, &m_ShowConsole);
            ImGui::MenuItem("Sensor overlays", nullptr, &m_ShowSensorOverlays);
            ImGui::MenuItem("Camera preview", nullptr, &m_ShowPosePanel);
            ImGui::MenuItem(
                "Animation panel",
                nullptr,
                &m_ShowAnimationPanel);
            ImGui::MenuItem("Robot telemetry", nullptr, &m_ShowTelemetry);
            ImGui::EndMenu();
        }

        const float statusWidth = 270.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - statusWidth);
        drawStatusBadge(
            m_SimulationRunning ? "● SIM RUNNING" : "● SIM PAUSED",
            m_SimulationRunning
                ? ImVec4(0.82f, 0.48f, 0.08f, 1.0f)
                : ImVec4(0.30f, 0.38f, 0.48f, 1.0f));
        ImGui::SameLine();
        drawStatusBadge(
            m_TelemetryClient.connected() && !m_TelemetryClient.stale()
                ? "ROS2 LIVE"
                : "ROS2 READ-ONLY",
            m_TelemetryClient.connected() && !m_TelemetryClient.stale()
                ? ImVec4(0.15f, 0.58f, 0.36f, 1.0f)
                : ImVec4(0.55f, 0.36f, 0.36f, 1.0f));
        ImGui::SameLine();
        drawStatusBadge("SAFE", ImVec4(0.15f, 0.58f, 0.36f, 1.0f));

        ImGui::EndMainMenuBar();
    }

    void Editor::drawScene()
    {
        ImGui::Begin("World", nullptr, ImGuiWindowFlags_NoTitleBar);

        drawPanelHeader("[R]", "ROBOT MODEL", "LINK TREE");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22.0f);
        ImGui::SmallButton("+");
        ImGui::TextDisabled("LINKS / JOINTS");

        if (ImGui::TreeNodeEx("world", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "World"))
        {
            const char *robotName = m_Robot && m_Robot->isLoaded()
                                        ? m_Robot->name().c_str()
                                        : "No robot loaded";

            const bool robotSelected = m_SelectedLink.empty() || m_SelectedLink == "robot";
            if (ImGui::TreeNodeEx("robot", ImGuiTreeNodeFlags_DefaultOpen | (robotSelected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", robotName))
            {
                if (m_Robot && m_Robot->isLoaded())
                {
                    for (const auto &[linkName, link] : m_Robot->data().links)
                    {
                        ImGui::PushID(linkName.c_str());
                        ImGui::TreeNodeEx(
                            "link",
                            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                ImGuiTreeNodeFlags_SpanAvailWidth |
                                (m_SelectedLink == linkName ? ImGuiTreeNodeFlags_Selected : 0),
                            "[L] %s",
                            linkName.c_str());
                        if (ImGui::IsItemClicked())
                            m_SelectedLink = linkName;
                        ImGui::PopID();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreeNodeEx("sun", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "sun");
            ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("SIMULATION STATUS");
        ImGui::Text("Physics");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48.0f);
        ImGui::TextColored(ImVec4(0.30f, 0.78f, 0.70f, 1.0f), "Ready");

        ImGui::End();
    }

    void Editor::drawViewport()
    {
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoTitleBar);

        drawPanelHeader("[3D]", "SCENE VIEWPORT", "PERSPECTIVE");
        ImGui::SameLine();
        ImGui::TextDisabled("| Tienkung Robot | FPS");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 340.0f);
        if (ImGui::SmallButton("Front"))
            m_CameraResetRequested = true;
        ImGui::SameLine();
        if (ImGui::SmallButton(m_ShowGrid ? "Grid ON" : "Grid OFF"))
            m_ShowGrid = !m_ShowGrid;
        ImGui::SameLine();
        if (ImGui::Button(m_SimulationRunning ? "Pause" : "Play"))
            m_SimulationRunning = !m_SimulationRunning;
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
            m_CameraResetRequested = true;
        ImGui::Separator();

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        int width = static_cast<int>(viewportSize.x);
        int height = static_cast<int>(viewportSize.y);

        if (width > 0 && height > 0)
        {
            m_RequestedSceneWidth = width;
            m_RequestedSceneHeight = height;

            ImGui::Image(
                static_cast<ImTextureID>(
                    static_cast<uintptr_t>(
                        m_SceneFramebuffer.getColorTexture())),
                viewportSize,
                ImVec2(0, 1),
                ImVec2(1, 0));

            const ImVec2 sceneViewportMin = ImGui::GetItemRectMin();
            const ImVec2 sceneViewportMax = ImGui::GetItemRectMax();
            m_ViewportHovered = ImGui::IsItemHovered();

            if (m_Robot && m_Robot->isLoaded() &&
                !m_SelectedLink.empty() &&
                m_Robot->jointForChildLink(m_SelectedLink))
            {
                ImGuizmo::SetDrawlist();
                ImGuizmo::Enable(true);
                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetRect(
                    ImGui::GetItemRectMin().x,
                    ImGui::GetItemRectMin().y,
                    viewportSize.x,
                    viewportSize.y);

                glm::mat4 jointFrame =
                    m_Robot->jointFrameWorldTransform(m_SelectedLink, m_RobotTransform);
                ImGuizmo::Manipulate(
                    glm::value_ptr(m_GizmoView),
                    glm::value_ptr(m_GizmoProjection),
                    ImGuizmo::ROTATE | ImGuizmo::TRANSLATE,
                    ImGuizmo::LOCAL,
                    glm::value_ptr(jointFrame));
                if (ImGuizmo::IsUsing())
                {
                    m_Robot->setJointFrameWorldTransform(
                        m_SelectedLink,
                        m_RobotTransform,
                        jointFrame);
                    m_SimulationRunning = false;
                }
            }

            ImGui::SetCursorPos(ImVec2(18.0f, 58.0f));
            ImGui::BeginChild("ViewportOverlay", ImVec2(250.0f, 142.0f), true);
            ImGui::TextDisabled("SIMULATION");
            ImGui::Text("State  %s", m_SimulationRunning ? "RUNNING" : "PAUSED");
            ImGui::Text("Time   %.2f / %.2f s", m_AnimationTime, m_AnimationDuration);
            ImGui::Text("Grid   %s", m_ShowGrid ? "ON" : "OFF");
            ImGui::Text("Sensors %s", m_ShowSensors ? "DEBUG" : "OFF");
            ImGui::TextDisabled("CAMERA");
            ImGui::Text("RMB orbit | MMB pan");
            ImGui::Text("Wheel or +/- zoom");
            ImGui::EndChild();

            if (m_ShowSensorOverlays)
            {
                const ImVec2 viewportMin = sceneViewportMin;
                const ImVec2 viewportMax = sceneViewportMax;
                const float cardWidth = glm::min(220.0f, viewportSize.x * 0.30f);
                const float cardHeight = cardWidth * 0.64f;
                const float cardGap = 8.0f;
                const float right = viewportMax.x - 14.0f;
                const float top = viewportMin.y + 14.0f;
                ImDrawList *drawList = ImGui::GetWindowDrawList();

                const auto drawSensorCard = [drawList,
                                             cardWidth,
                                             cardHeight,
                                             right,
                                             cardGap,
                                             top,
                                             this](
                                                const char *title,
                                                ImTextureID texture,
                                                int imageWidth,
                                                int imageHeight,
                                                bool available,
                                                float y)
                {
                    const ImVec2 cardMin(right - cardWidth, y);
                    const ImVec2 cardMax(right, y + cardHeight);
                    drawList->AddRectFilled(cardMin, cardMax, kViewportCardBackground, 5.0f);
                    drawList->AddRect(
                        cardMin,
                        cardMax,
                        kViewportCardBorder,
                        5.0f);
                    drawList->AddText(
                        ImVec2(cardMin.x + 9.0f, cardMin.y + 6.0f),
                        kViewportText,
                        title);
                    drawList->AddText(
                        ImVec2(cardMax.x - 28.0f, cardMin.y + 6.0f),
                        kViewportMutedText,
                        "[+]");

                    const ImVec2 imageMin(cardMin.x + 7.0f, cardMin.y + 25.0f);
                    const ImVec2 imageMax(cardMax.x - 7.0f, cardMax.y - 7.0f);
                    drawList->AddRectFilled(
                        imageMin,
                        imageMax,
                        kViewportImageBackground,
                        3.0f);
                    if (available && imageWidth > 0 && imageHeight > 0)
                    {
                        const float sourceAspect =
                            static_cast<float>(imageWidth) /
                            static_cast<float>(imageHeight);
                        const float canvasAspect =
                            (imageMax.x - imageMin.x) / (imageMax.y - imageMin.y);
                        ImVec2 imageSize(
                            imageMax.x - imageMin.x,
                            imageMax.y - imageMin.y);
                        if (sourceAspect > canvasAspect)
                            imageSize.y = imageSize.x / sourceAspect;
                        else
                            imageSize.x = imageSize.y * sourceAspect;
                        const ImVec2 centeredMin(
                            imageMin.x + ((imageMax.x - imageMin.x) - imageSize.x) * 0.5f,
                            imageMin.y + ((imageMax.y - imageMin.y) - imageSize.y) * 0.5f);
                        drawList->AddImage(
                            texture,
                            centeredMin,
                            ImVec2(centeredMin.x + imageSize.x, centeredMin.y + imageSize.y),
                            ImVec2(0.0f, 0.0f),
                            ImVec2(1.0f, 1.0f));
                    }
                    else
                    {
                        const char *status = m_TelemetryClient.connected()
                                                 ? "NO SIGNAL"
                                                 : "DISCONNECTED";
                        const ImVec2 statusSize = ImGui::CalcTextSize(status);
                        drawList->AddText(
                            ImVec2(
                                imageMin.x + ((imageMax.x - imageMin.x) - statusSize.x) * 0.5f,
                                imageMin.y + ((imageMax.y - imageMin.y) - statusSize.y) * 0.5f),
                            kViewportMutedText,
                            status);
                    }

                    ImGui::PushID(title);
                    ImGui::SetCursorScreenPos(cardMin);
                    if (ImGui::InvisibleButton(
                            "SensorCard",
                            ImVec2(cardWidth, cardHeight)))
                        m_ShowPosePanel = true;
                    ImGui::PopID();
                };

                const bool sensorLinkLive =
                    m_TelemetryClient.connected() && !m_TelemetryClient.stale();
                drawSensorCard(
                    "RGB CAMERA",
                    static_cast<ImTextureID>(
                        static_cast<uintptr_t>(m_RemoteCameraTexture)),
                    m_RemoteCameraWidth,
                    m_RemoteCameraHeight,
                    sensorLinkLive && m_RemoteCameraTexture != 0,
                    top);
                drawSensorCard(
                    "DEPTH",
                    static_cast<ImTextureID>(
                        static_cast<uintptr_t>(m_RemoteDepthTexture)),
                    m_RemoteDepthWidth,
                    m_RemoteDepthHeight,
                    sensorLinkLive && m_RemoteDepthTexture != 0,
                    top + cardHeight + cardGap);
            }
        }
        ImGui::End();
    }

    void Editor::drawInspector()
    {
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoTitleBar);

        const std::string selectedName =
            m_SelectedLink.empty()
                ? (m_Robot && m_Robot->isLoaded() ? m_Robot->name() : "No selection")
                : m_SelectedLink;
        drawPanelHeader("[I]", "LINK INSPECTOR", selectedName.c_str());
        ImGui::Text("%s", selectedName.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 38.0f);
        ImGui::TextDisabled("LINK");
        ImGui::Separator();

        ImGui::TextDisabled("General");
        ImGui::Text("Name");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", selectedName.c_str());

        if (ImGui::CollapsingHeader("Transform"))
        {
            ImGui::DragFloat3(
                "Position",
                m_Position,
                0.1f);

            ImGui::DragFloat3(
                "Rotation",
                m_Rotation,
                1.0f);

            ImGui::DragFloat3(
                "Scale",
                m_Scale,
                0.1f);
        }

        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BulletText("Rigid body");
            ImGui::BulletText("Differential drive");
            ImGui::BulletText("Lidar sensor");
        }

        if (const UrdfJoint *joint = m_Robot && m_Robot->isLoaded()
                                         ? m_Robot->jointForChildLink(m_SelectedLink)
                                         : nullptr)
        {
            if (ImGui::CollapsingHeader("Joint direction", ImGuiTreeNodeFlags_DefaultOpen))
            {
                glm::vec3 axis = joint->axis;
                if (ImGui::DragFloat3("Axis", &axis.x, 0.01f, -1.0f, 1.0f))
                    m_Robot->setJointAxis(joint->name, axis);
                ImGui::TextDisabled("Use the gizmo to edit the joint frame.");
            }
        }

        if (ImGui::CollapsingHeader("Sensors"))
        {
            ImGui::Text("lidar");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
            ImGui::TextColored(ImVec4(0.30f, 0.78f, 0.70f, 1.0f), "ON");
            ImGui::Text("camera");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
            ImGui::TextColored(ImVec4(0.30f, 0.78f, 0.70f, 1.0f), "ON");
        }

        ImGui::End();
    }

    void Editor::drawMotion()
    {
        if (!m_ShowAnimationPanel)
            return;

        ImGui::SetNextWindowSize(
            ImVec2(560.0f, 760.0f),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(
            ImVec2(
                ImGui::GetMainViewport()->Pos.x +
                    ImGui::GetMainViewport()->Size.x - 584.0f,
                ImGui::GetMainViewport()->Pos.y + 72.0f),
            ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(
                "Animation",
                &m_ShowAnimationPanel,
                ImGuiWindowFlags_NoDocking))
        {
            ImGui::End();
            return;
        }

        drawPanelHeader(
            "[J]",
            "MOTION",
            m_SimulationRunning ? "PLAYBACK" : "MANUAL EDITING");

        if (ImGui::CollapsingHeader(
                "Animation playback",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::string &activeAnimation =
                m_AnimationMixer.currentAnimation();
            ImGui::Text(
                "Active clip: %s",
                activeAnimation.empty() ? "None" : activeAnimation.c_str());
            ImGui::TextDisabled(
                "Priority %d  |  queued events %zu",
                m_AnimationMixer.priority(),
                m_AnimationEventQueue.size());

            if (ImGui::Button(m_SimulationRunning ? "Pause" : "Play"))
                m_SimulationRunning = !m_SimulationRunning;
            ImGui::SameLine();
            if (ImGui::Button("Step"))
            {
                m_SimulationRunning = false;
                setSimulationTime(m_AnimationTime + (1.0f / 60.0f));
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset"))
                resetSimulation();

            const bool timelineChanged = ImGui::SliderFloat(
                "Timeline",
                &m_AnimationTime,
                0.0f,
                m_AnimationDuration,
                "%.2f s");
            if (timelineChanged)
            {
                m_SimulationRunning = false;
                setSimulationTime(m_AnimationTime);
            }
            ImGui::TextDisabled(
                "%.2f / %.2f seconds",
                m_AnimationTime,
                m_AnimationDuration);
            ImGui::Checkbox("Loop playback", &m_LoopAnimation);
        }

        if (ImGui::CollapsingHeader(
                "Animation events",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled(
                "Events are prioritized and safety-checked before playback.");
            if (ImGui::Button("Greeting"))
                dispatchAnimationEvent(
                    {AnimationEventType::Greeting, 10, "editor"});
            ImGui::SameLine();
            if (ImGui::Button("Idle"))
                dispatchAnimationEvent(
                    {AnimationEventType::Idle, 0, "editor"});
            ImGui::SameLine();
            if (ImGui::Button("Alert"))
                dispatchAnimationEvent(
                    {AnimationEventType::Alert, 50, "editor"});
        }

        if (ImGui::CollapsingHeader(
                "Safety state",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox(
                "Harness attached",
                &m_AnimationSafety.harnessAttached);
            ImGui::Checkbox(
                "Ground contact",
                &m_AnimationSafety.groundContact);
            ImGui::Checkbox(
                "Motors enabled",
                &m_AnimationSafety.motorsEnabled);
            ImGui::Checkbox(
                "Emergency stop",
                &m_AnimationSafety.emergencyStop);
            const bool safetyAllows =
                !m_AnimationSafety.emergencyStop &&
                m_AnimationSafety.motorsEnabled;
            ImGui::TextColored(
                safetyAllows
                    ? ImVec4(0.18f, 0.62f, 0.28f, 1.0f)
                    : ImVec4(0.75f, 0.18f, 0.16f, 1.0f),
                "%s",
                safetyAllows
                    ? "Motion permitted by current state"
                    : "Motion blocked by current state");
        }

        if (ImGui::CollapsingHeader(
                "Manual pose editing",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox(
                "Animate selected component only",
                &m_AnimateSelectedOnly);
            if (m_AnimateSelectedOnly)
            {
                const UrdfJoint *selectedJoint =
                    m_Robot && m_Robot->isLoaded()
                        ? m_Robot->jointForChildLink(m_SelectedLink)
                        : nullptr;
                ImGui::TextDisabled(
                    selectedJoint
                        ? "Selected joint: %s"
                        : "Select a movable link to animate",
                    selectedJoint ? selectedJoint->name.c_str() : "");
            }
        }

        if (!m_AnimationStatus.empty())
        {
            ImGui::Separator();
            ImGui::TextDisabled("Status");
            ImGui::TextWrapped("%s", m_AnimationStatus.c_str());
        }

        if (!m_Robot || !m_Robot->isLoaded())
        {
            ImGui::TextDisabled("No robot loaded");
        }
        else
        {
            const UrdfJoint *selectedJoint =
                m_Robot->jointForChildLink(m_SelectedLink);
            ImGui::TextDisabled(
                selectedJoint ? "Selected joint: %s" : "Select a link to edit its joint",
                selectedJoint ? selectedJoint->name.c_str() : "");

            const float controlsHeight = glm::max(80.0f, ImGui::GetContentRegionAvail().y - 34.0f);
            ImGui::BeginChild(
                "JointControls",
                ImVec2(0.0f, controlsHeight),
                false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar);

            for (const auto &[jointName, joint] : m_Robot->data().joints)
            {
                if (joint.type == UrdfJoint::Type::FIXED)
                    continue;

                float position = m_Robot->jointPosition(jointName);
                float lower = -3.14159f;
                float upper = 3.14159f;
                const char *format = "%.2f rad";

                if (joint.type == UrdfJoint::Type::PRISMATIC)
                {
                    lower = -1.0f;
                    upper = 1.0f;
                    format = "%.2f m";
                }
                if (joint.limit.has_limit)
                {
                    lower = static_cast<float>(joint.limit.lower);
                    upper = static_cast<float>(joint.limit.upper);
                }

                const bool jointSelected =
                    selectedJoint && selectedJoint->name == jointName;
                ImGui::PushID(jointName.c_str());
                ImGui::BeginGroup();
                ImGui::TextColored(
                    jointSelected
                        ? ImVec4(0.08f, 0.35f, 0.70f, 1.0f)
                        : ImVec4(0.25f, 0.30f, 0.36f, 1.0f),
                    "%s",
                    jointName.c_str());
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 62.0f);
                ImGui::TextDisabled("%s", joint.type == UrdfJoint::Type::PRISMATIC ? "PRISM" : "REV");
                if (jointSelected)
                {
                    ImGui::PushStyleColor(
                        ImGuiCol_FrameBg,
                        ImVec4(0.64f, 0.78f, 0.94f, 1.0f));
                    ImGui::PushStyleColor(
                        ImGuiCol_SliderGrab,
                        ImVec4(0.12f, 0.42f, 0.72f, 1.0f));
                }

                if (ImGui::SliderFloat(
                        "##value",
                        &position,
                        lower,
                        upper,
                        format))
                {
                    m_Robot->setJointPosition(jointName, position);
                    m_SimulationRunning = false;
                }

                if (ImGui::IsItemClicked())
                    m_SelectedLink = joint.child_link;

                if (jointSelected)
                    ImGui::PopStyleColor(2);
                ImGui::SameLine();
                ImGui::TextDisabled("%.2f", position);
                ImGui::EndGroup();
                ImGui::PopID();
            }

            ImGui::EndChild();

            if (ImGui::Button("Reset joints"))
            {
                resetSimulation();
            }
        }

        ImGui::End();
    }

    void Editor::drawConsole()
    {
        if (!m_ShowConsole)
            return;

        ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoTitleBar);

        drawPanelHeader("[!]", "DIAGNOSTICS", "LOCAL");
        ImGui::TextDisabled("OUTPUT");
        ImGui::SameLine();
        ImGui::TextDisabled("PHYSICS");
        ImGui::SameLine();
        ImGui::TextDisabled("SENSORS");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.42f, 0.72f, 0.75f, 1.0f), "[00:00:00] World loaded: robot_alpha");
        ImGui::Text("[00:00:01] Physics system ready (0 contacts)");
        ImGui::Text("[00:00:01] Sensors online: lidar, camera");

        ImGui::End();
    }

    void Editor::drawTelemetry()
    {
        if (!m_ShowTelemetry)
            return;

        ImGui::Begin("Robot telemetry", &m_ShowTelemetry);
        drawPanelHeader(
            "[ROS]",
            "ROBOT TELEMETRY",
            m_TelemetryClient.connected() ? "READ-ONLY LINK" : "DISCONNECTED");

        ImGui::TextDisabled(
            "Relay: %s:%u",
            m_TelemetryClient.host().c_str(),
            static_cast<unsigned>(m_TelemetryClient.port()));
        ImGui::SameLine();
        if (ImGui::Button(m_TelemetryClient.connected() ? "Reconnect" : "Connect"))
            m_TelemetryClient.connect(m_TelemetryClient.host(), m_TelemetryClient.port());
        ImGui::SameLine();
        if (ImGui::Button("Disconnect"))
            m_TelemetryClient.shutdown();
        ImGui::SameLine();
        if (ImGui::SmallButton("Sensor overlays"))
            m_ShowSensorOverlays = !m_ShowSensorOverlays;

        const std::string telemetryError = m_TelemetryClient.lastError();
        if (!telemetryError.empty())
            ImGui::TextColored(
                ImVec4(0.68f, 0.24f, 0.20f, 1.0f),
                "%s",
                telemetryError.c_str());

        const auto &snapshot = m_TelemetryClient.snapshot();
        const bool live = m_TelemetryClient.connected() && !m_TelemetryClient.stale();
        ImGui::TextColored(
            live ? ImVec4(0.15f, 0.58f, 0.36f, 1.0f)
                 : ImVec4(0.55f, 0.36f, 0.36f, 1.0f),
            live ? "LIVE DATA" : "WAITING FOR TELEMETRY");
        ImGui::SameLine();
        ImGui::TextDisabled("Frames: %llu", static_cast<unsigned long long>(snapshot.sequence));
        ImGui::Separator();

        ImGui::Text(
            "Body control: %s",
            snapshot.bodyControlState == 1 ? "RUNNING" : "UNKNOWN/OFFLINE");
        ImGui::Text(
            "Process manager: %s",
            snapshot.processState == 1 ? "RUNNING" : "UNKNOWN/OFFLINE");
        ImGui::Text(
            "Battery: %.1f V  %.1f A  %.1f W",
            snapshot.batteryVoltage,
            snapshot.batteryCurrent,
            snapshot.batteryPower);
        ImGui::Text(
            "IMU R/P/Y: %.3f / %.3f / %.3f",
            snapshot.imuRoll,
            snapshot.imuPitch,
            snapshot.imuYaw);
        ImGui::Checkbox(
            "Apply healthy motor feedback to model",
            &m_ApplyTelemetryToRobot);
        ImGui::SameLine();
        ImGui::TextDisabled("(read-only, position units not converted)");
        ImGui::Separator();

        if (ImGui::BeginTable(
                "motorTelemetry",
                7,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY,
                ImVec2(0.0f, glm::max(100.0f, ImGui::GetContentRegionAvail().y - 116.0f))))
        {
            const char *columns[] = {"Motor", "Group", "Position", "Speed", "Current", "Temp", "State"};
            ImGui::TableSetupColumn(columns[0]);
            ImGui::TableSetupColumn(columns[1]);
            ImGui::TableSetupColumn(columns[2]);
            ImGui::TableSetupColumn(columns[3]);
            ImGui::TableSetupColumn(columns[4]);
            ImGui::TableSetupColumn(columns[5]);
            ImGui::TableSetupColumn(columns[6]);
            ImGui::TableHeadersRow();
            for (const auto &motor : snapshot.motors)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", motor.id);
                ImGui::TableNextColumn();
                const char *group =
                    motor.id < 10 ? "Head" : motor.id < 20 ? "Left arm"
                                         : motor.id < 30   ? "Right arm"
                                         : motor.id == 31  ? "Waist"
                                         : motor.id < 60   ? "Left leg"
                                                           : "Right leg";
                ImGui::TextUnformatted(group);
                ImGui::TableNextColumn();
                ImGui::Text("%.4f", motor.position);
                ImGui::TableNextColumn();
                ImGui::Text("%.4f", motor.speed);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", motor.current);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f C", motor.temperature);
                ImGui::TableNextColumn();
                if (motor.error == 0)
                    ImGui::TextColored(
                        ImVec4(0.15f, 0.58f, 0.36f, 1.0f),
                        "OK");
                else
                    ImGui::TextColored(
                        ImVec4(0.68f, 0.24f, 0.20f, 1.0f),
                        "%u",
                        motor.error);
            }
            ImGui::EndTable();
        }

        ImGui::TextDisabled(
            "Read-only feedback. Position units are vendor-reported; no conversion is applied.");
        ImGui::End();
    }

    void Editor::drawPosePanel()
    {
        if (!m_ShowPosePanel)
            return;

        ImGui::SetNextWindowSize(ImVec2(420.0f, 460.0f), ImGuiCond_FirstUseEver);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kViewportPanelBackground);
        ImGui::Begin(
            "Camera preview",
            &m_ShowPosePanel,
            ImGuiWindowFlags_NoDocking);

        ImGui::TextColored(kViewportAccent, "[CAM]");
        ImGui::SameLine();
        ImGui::TextUnformatted("CAMERA PREVIEW");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 64.0f);
        ImGui::TextColored(
            kHealthyStatus,
            "READY");
        ImGui::Separator();
        const bool remoteCameraAvailable =
            m_RemoteCameraTexture != 0 &&
            m_TelemetryClient.connected() &&
            !m_TelemetryClient.stale();
        const bool remoteDepthAvailable =
            m_RemoteDepthTexture != 0 &&
            m_TelemetryClient.connected() &&
            !m_TelemetryClient.stale();
        ImGui::TextDisabled(
            remoteCameraAvailable || remoteDepthAvailable
                ? "ROS 2 camera and depth streams (read-only)."
                : "Waiting for the ROS 2 camera stream.");

        if (ImGui::BeginTabBar("SensorPreviewTabs"))
        {
            if (ImGui::BeginTabItem("RGB"))
            {
                if (remoteCameraAvailable)
                    ImGui::TextDisabled(
                        "Received %d x %d | latest frame only",
                        m_RemoteCameraWidth,
                        m_RemoteCameraHeight);
                else
                    ImGui::TextDisabled("RGB stream unavailable");
                const ImVec2 canvasPosition = ImGui::GetCursorScreenPos();
                const ImVec2 canvasSize(
                    ImGui::GetContentRegionAvail().x,
                    glm::max(220.0f, ImGui::GetContentRegionAvail().y - 52.0f));
                ImGui::InvisibleButton("RgbCameraCanvas", canvasSize);
                ImDrawList *drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(
                    canvasPosition,
                    ImVec2(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y),
                    kViewportImageBackground,
                    4.0f);
                if (remoteCameraAvailable)
                {
                    const float sourceAspect =
                        static_cast<float>(m_RemoteCameraWidth) /
                        static_cast<float>(m_RemoteCameraHeight);
                    const float canvasAspect = canvasSize.x / canvasSize.y;
                    ImVec2 imageSize = canvasSize;
                    if (sourceAspect > canvasAspect)
                        imageSize.y = canvasSize.x / sourceAspect;
                    else
                        imageSize.x = canvasSize.y * sourceAspect;
                    const ImVec2 imagePosition(
                        canvasPosition.x + (canvasSize.x - imageSize.x) * 0.5f,
                        canvasPosition.y + (canvasSize.y - imageSize.y) * 0.5f);
                    drawList->AddImage(
                        static_cast<ImTextureID>(
                            static_cast<uintptr_t>(m_RemoteCameraTexture)),
                        imagePosition,
                        ImVec2(imagePosition.x + imageSize.x, imagePosition.y + imageSize.y),
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        kTextureTint);
                }
                else if (m_CameraCapture.isOpen())
                {
                    drawList->AddImage(
                        static_cast<ImTextureID>(
                            static_cast<uintptr_t>(m_CameraCapture.texture())),
                        canvasPosition,
                        ImVec2(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y),
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        kTextureTint);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Depth"))
            {
                if (remoteDepthAvailable)
                    ImGui::TextDisabled(
                        "Received %d x %d | downsampled relay frame",
                        m_RemoteDepthWidth,
                        m_RemoteDepthHeight);
                else
                    ImGui::TextDisabled("Depth stream unavailable");
                const ImVec2 canvasPosition = ImGui::GetCursorScreenPos();
                const ImVec2 canvasSize(
                    ImGui::GetContentRegionAvail().x,
                    glm::max(220.0f, ImGui::GetContentRegionAvail().y - 52.0f));
                ImGui::InvisibleButton("DepthCameraCanvas", canvasSize);
                ImDrawList *drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(
                    canvasPosition,
                    ImVec2(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y),
                    kViewportImageBackground,
                    4.0f);
                if (remoteDepthAvailable)
                {
                    const float sourceAspect =
                        static_cast<float>(m_RemoteDepthWidth) /
                        static_cast<float>(m_RemoteDepthHeight);
                    const float canvasAspect = canvasSize.x / canvasSize.y;
                    ImVec2 imageSize = canvasSize;
                    if (sourceAspect > canvasAspect)
                        imageSize.y = canvasSize.x / sourceAspect;
                    else
                        imageSize.x = canvasSize.y * sourceAspect;
                    const ImVec2 imagePosition(
                        canvasPosition.x + (canvasSize.x - imageSize.x) * 0.5f,
                        canvasPosition.y + (canvasSize.y - imageSize.y) * 0.5f);
                    drawList->AddImage(
                        static_cast<ImTextureID>(
                            static_cast<uintptr_t>(m_RemoteDepthTexture)),
                        imagePosition,
                        ImVec2(imagePosition.x + imageSize.x, imagePosition.y + imageSize.y),
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        kTextureTint);
                }
                else
                {
                    ImGui::TextDisabled("Waiting for /camera/depth/image_raw.");
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::TextDisabled(
            remoteCameraAvailable || remoteDepthAvailable
                ? "Live ROS 2 feed. Latest frames only; no full-resolution request is made."
            : m_CameraCapture.isOpen()
                ? "Live local camera feed."
                : m_CameraCapture.lastError().c_str());
        ImGui::End();
        ImGui::PopStyleColor();
    }

    void Editor::endFrame()
    {
        if (!m_Initialized)
            return;

        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData());

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow *currentContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(currentContext);
        }
    }

    void Editor::shutdown()
    {
        if (!m_Initialized)
            return;

        if (m_RemoteCameraTexture != 0)
        {
            glDeleteTextures(1, &m_RemoteCameraTexture);
            m_RemoteCameraTexture = 0;
        }
        if (m_RemoteDepthTexture != 0)
        {
            glDeleteTextures(1, &m_RemoteDepthTexture);
            m_RemoteDepthTexture = 0;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImGui::DestroyContext();

        m_Initialized = false;
    }

}