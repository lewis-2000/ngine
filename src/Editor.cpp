#include "Editor.h"

#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <filesystem>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include "Input.h"

namespace Mara
{
    namespace
    {
        struct MotorJointMapping
        {
            int motorId;
            const char *jointName;
            float modelSign;
        };

        constexpr std::array<MotorJointMapping, 7> kLeftArmMappings{{
            {11, "left_joint1", 1.0f},
            {12, "shoulder_roll_l_joint", 1.0f},
            {13, "left_joint3", 1.0f},
            {14, "elbow_l_joint", 1.0f},
            {15, "left_joint5", 1.0f},
            {16, "left_joint6", 1.0f},
            {17, "left_joint7", 1.0f},
        }};

        constexpr std::array<MotorJointMapping, 7> kRightArmMappings{{
            {21, "right_joint1", -1.0f},
            {22, "shoulder_roll_r_joint", -1.0f},
            {23, "right_joint3", -1.0f},
            {24, "elbow_r_joint", -1.0f},
            {25, "right_joint5", -1.0f},
            {26, "right_joint6", -1.0f},
            {27, "right_joint7", -1.0f},
        }};

        constexpr std::array<MotorJointMapping, 6> kLeftLegMappings{{
            {51, "hip_roll_l_joint", 1.0f},
            {52, "hip_yaw_l_joint", 1.0f},
            {53, "hip_pitch_l_joint", 1.0f},
            {54, "knee_pitch_l_joint", 1.0f},
            {55, "ankle_pitch_l_joint", 1.0f},
            {56, "ankle_roll_l_joint", 1.0f},
        }};

        constexpr std::array<MotorJointMapping, 6> kRightLegMappings{{
            {61, "hip_roll_r_joint", -1.0f},
            {62, "hip_yaw_r_joint", 1.0f},
            {63, "hip_pitch_r_joint", 1.0f},
            {64, "knee_pitch_r_joint", 1.0f},
            {65, "ankle_pitch_r_joint", 1.0f},
            {66, "ankle_roll_r_joint", -1.0f},
        }};

        bool intersectRayBox(
            const glm::vec3 &origin,
            const glm::vec3 &direction,
            const glm::vec3 &minimum,
            const glm::vec3 &maximum,
            float &distance)
        {
            float nearDistance = 0.0f;
            float farDistance = std::numeric_limits<float>::max();
            for (int axis = 0; axis < 3; ++axis)
            {
                if (std::abs(direction[axis]) <= 0.0001f)
                {
                    if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
                        return false;
                    continue;
                }
                const float inverseDirection = 1.0f / direction[axis];
                float axisNear = (minimum[axis] - origin[axis]) * inverseDirection;
                float axisFar = (maximum[axis] - origin[axis]) * inverseDirection;
                if (axisNear > axisFar)
                    std::swap(axisNear, axisFar);
                nearDistance = std::max(nearDistance, axisNear);
                farDistance = std::min(farDistance, axisFar);
                if (nearDistance > farDistance)
                    return false;
            }
            if (farDistance < 0.0f)
                return false;
            distance = nearDistance >= 0.0f ? nearDistance : farDistance;
            return distance >= 0.0f;
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
        ImGui::StyleColorsLight();

        ImGuiStyle &style = ImGui::GetStyle();

        // ------------------------------------------------------------
        // Geometry
        // ------------------------------------------------------------

        style.WindowRounding = 10.0f;
        style.WindowMenuButtonPosition = ImGuiDir_None; // Hide the default title-bar window/menu drag icon.
        style.DockingSeparatorSize = 5.0f;
        style.ChildRounding = 9.0f;
        style.FrameRounding = 6.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.TabRounding = 7.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;

        style.WindowPadding = ImVec2(16.0f, 14.0f);
        style.FramePadding = ImVec2(10.0f, 7.0f);
        style.ItemSpacing = ImVec2(9.0f, 7.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.CellPadding = ImVec2(7.0f, 5.0f);
        style.IndentSpacing = 18.0f;
        style.TabBarBorderSize = 1.0f;

        style.ScrollbarSize = 13.0f;
        style.GrabMinSize = 11.0f;

        // ------------------------------------------------------------
        // Colors
        // ------------------------------------------------------------

        ImVec4 *colors = style.Colors;

        // Background hierarchy
        colors[ImGuiCol_WindowBg] =
            ImVec4(0.95f, 0.96f, 0.97f, 1.00f);

        colors[ImGuiCol_ChildBg] =
            ImVec4(0.98f, 0.985f, 0.995f, 1.00f);

        colors[ImGuiCol_PopupBg] =
            ImVec4(0.99f, 0.99f, 1.00f, 0.99f);

        colors[ImGuiCol_MenuBarBg] =
            ImVec4(0.86f, 0.88f, 0.92f, 1.00f);

        // Window title bars
        colors[ImGuiCol_TitleBg] =
            ImVec4(0.86f, 0.88f, 0.92f, 1.00f);

        colors[ImGuiCol_TitleBgActive] =
            ImVec4(0.76f, 0.84f, 0.94f, 1.00f);

        colors[ImGuiCol_TitleBgCollapsed] =
            ImVec4(0.86f, 0.88f, 0.92f, 1.00f);

        // Borders / separators
        colors[ImGuiCol_Border] =
            ImVec4(0.78f, 0.80f, 0.84f, 1.00f);

        colors[ImGuiCol_BorderShadow] =
            ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        colors[ImGuiCol_Separator] =
            ImVec4(0.68f, 0.70f, 0.75f, 0.75f);

        colors[ImGuiCol_SeparatorHovered] =
            ImVec4(0.32f, 0.50f, 0.78f, 0.75f);

        colors[ImGuiCol_SeparatorActive] =
            ImVec4(0.20f, 0.42f, 0.75f, 1.00f);

        // Text
        colors[ImGuiCol_Text] =
            ImVec4(0.10f, 0.11f, 0.13f, 1.00f);

        colors[ImGuiCol_TextDisabled] =
            ImVec4(0.38f, 0.42f, 0.50f, 1.00f);

        // Controls
        colors[ImGuiCol_FrameBg] =
            ImVec4(0.86f, 0.88f, 0.92f, 1.00f);

        colors[ImGuiCol_FrameBgHovered] =
            ImVec4(0.78f, 0.84f, 0.93f, 1.00f);

        colors[ImGuiCol_FrameBgActive] =
            ImVec4(0.64f, 0.75f, 0.90f, 1.00f);

        // Buttons
        colors[ImGuiCol_Button] =
            ImVec4(0.82f, 0.85f, 0.90f, 1.00f);

        colors[ImGuiCol_ButtonHovered] =
            ImVec4(0.68f, 0.78f, 0.91f, 1.00f);

        colors[ImGuiCol_ButtonActive] =
            ImVec4(0.45f, 0.62f, 0.84f, 1.00f);

        colors[ImGuiCol_ResizeGrip] =
            ImVec4(0.35f, 0.52f, 0.76f, 0.30f);
        colors[ImGuiCol_ResizeGripHovered] =
            ImVec4(0.25f, 0.45f, 0.72f, 0.60f);

        // Headers / tree nodes
        colors[ImGuiCol_Header] =
            ImVec4(0.78f, 0.83f, 0.91f, 1.00f);

        colors[ImGuiCol_HeaderHovered] =
            ImVec4(0.65f, 0.76f, 0.90f, 1.00f);

        colors[ImGuiCol_HeaderActive] =
            ImVec4(0.48f, 0.65f, 0.86f, 1.00f);

        // Selection / accent
        colors[ImGuiCol_CheckMark] =
            ImVec4(0.16f, 0.38f, 0.68f, 1.00f);

        colors[ImGuiCol_SliderGrab] =
            ImVec4(0.25f, 0.48f, 0.78f, 1.00f);

        colors[ImGuiCol_SliderGrabActive] =
            ImVec4(0.16f, 0.36f, 0.65f, 1.00f);

        // Tabs
        colors[ImGuiCol_Tab] =
            ImVec4(0.82f, 0.85f, 0.90f, 1.00f);

        colors[ImGuiCol_TabHovered] =
            ImVec4(0.68f, 0.78f, 0.91f, 1.00f);

        colors[ImGuiCol_TabSelected] =
            ImVec4(0.70f, 0.80f, 0.93f, 1.00f);

        colors[ImGuiCol_TabSelectedOverline] =
            ImVec4(0.16f, 0.38f, 0.68f, 1.00f);

        colors[ImGuiCol_TabDimmed] =
            ImVec4(0.88f, 0.89f, 0.93f, 1.00f);

        colors[ImGuiCol_TabDimmedSelected] =
            ImVec4(0.76f, 0.82f, 0.91f, 1.00f);

        colors[ImGuiCol_TabUnfocused] = ImVec4(0.88f, 0.89f, 0.93f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.76f, 0.82f, 0.91f, 1.00f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.16f, 0.38f, 0.68f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.78f, 0.82f, 0.89f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.54f, 0.59f, 0.68f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.70f, 0.73f, 0.80f, 1.00f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.94f, 0.95f, 0.97f, 1.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.89f, 0.91f, 0.94f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.16f, 0.38f, 0.68f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.45f, 0.62f, 0.84f, 0.45f);
        // Hide the bright drag/drop target marker; dock-window menu/drag icon is disabled via WindowMenuButtonPosition.
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.20f, 0.92f, 0.98f, 0.0f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.16f, 0.38f, 0.68f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.40f, 0.43f, 0.50f, 0.25f);

        // Docking
        colors[ImGuiCol_DockingPreview] =
            ImVec4(0.25f, 0.48f, 0.78f, 0.35f);

        colors[ImGuiCol_DockingEmptyBg] =
            ImVec4(0.90f, 0.91f, 0.94f, 1.00f);

        // Optional font family setup. Keep the font files in resources/fonts.
        // The built-in font remains a safe fallback if any file is absent.
        ImFontConfig bodyConfig;
        bodyConfig.OversampleH = 3;
        bodyConfig.OversampleV = 2;
        bodyConfig.PixelSnapH = false;
        bodyConfig.RasterizerMultiply = 1.10f;

        ImFont *bodyFont = io.Fonts->AddFontFromFileTTF(
            "resources/fonts/IBMPlexSans-Regular.ttf", 20.0f, &bodyConfig);
        if (!bodyFont)
            bodyFont = io.Fonts->AddFontFromFileTTF(
                "resources/fonts/IBMPlexSans-Variable.ttf", 20.0f, &bodyConfig);

        ImFont *headingFont = io.Fonts->AddFontFromFileTTF(
            "resources/fonts/IBMPlexSans-SemiBold.ttf", 22.0f, &bodyConfig);
        if (!headingFont)
            headingFont = io.Fonts->AddFontFromFileTTF(
                "resources/fonts/IBMPlexSans-Variable.ttf", 22.0f, &bodyConfig);

        ImFont *monoFont = io.Fonts->AddFontFromFileTTF(
            "resources/fonts/IBMPlexMono-Regular.ttf", 18.0f, &bodyConfig);

        // Sized built-in fonts keep the UI legible even if the optional TTFs
        // have not yet been copied into the resources directory.
        const auto makeDefaultFont = [&io](float sizePixels) -> ImFont *
        {
            ImFontConfig config;
            config.SizePixels = sizePixels;
            return io.Fonts->AddFontDefault(&config);
        };

        m_FontBody = bodyFont ? bodyFont : makeDefaultFont(20.0f);
        m_FontHeading = headingFont ? headingFont : makeDefaultFont(22.0f);
        m_FontMono = monoFont ? monoFont : makeDefaultFont(18.0f);
        io.FontDefault = m_FontBody;

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        m_CameraCapture.open();
        m_PoseEstimator = createPoseEstimator();

        if (m_Robot && m_Robot->isLoaded())
            m_SelectedLink = m_Robot->rootLink();

        m_StopPathfindingThread = false;
        m_PathfindingThread = std::thread([this]
        {
            std::unique_lock<std::mutex> lock(m_PathMutex);
            while (true)
            {
                m_PathCondition.wait(
                    lock,
                    [this]
                    { return m_StopPathfindingThread || m_PathRequestPending; });
                if (m_StopPathfindingThread)
                    return;

                PathRequest request = std::move(m_PathRequest);
                m_PathRequestPending = false;
                m_PathSearchRunning = true;
                lock.unlock();

                std::vector<glm::vec3> result;
                const bool found = rebuildSimulationPath(request, result);

                lock.lock();
                if (!m_StopPathfindingThread)
                {
                    m_PathSearchRunning = false;
                    m_PathResult = std::move(result);
                    m_PathResultFound = found;
                    m_PathResultGeneration = request.generation;
                    m_PathResultReady = true;
                }
            }
        });
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
        generateSyntheticDepth();
        const bool hasLiveDepth =
            m_TelemetryClient.hasData() &&
            !m_TelemetryClient.stale() &&
            m_TelemetryClient.snapshot().depthSequence != 0;
        if (m_TelemetryClient.hasData())
        {
            const RosTelemetrySnapshot &snapshot = m_TelemetryClient.snapshot();
            if (snapshot.depthSequence != 0 &&
                snapshot.depthSequence != m_LastMappedDepthSequence)
            {
                m_OccupancyMap.integrateDepth(
                    snapshot.depth,
                    snapshot.depthWidth,
                    snapshot.depthHeight,
                    1.0f,
                    500.0f,
                    500.0f,
                    static_cast<float>(snapshot.depthWidth) * 0.5f,
                    static_cast<float>(snapshot.depthHeight) * 0.5f,
                    m_SimulatedRobotPosition,
                    m_SimulatedRobotYaw);
                m_LastMappedDepthSequence = snapshot.depthSequence;
            }
        }
        else if (!hasLiveDepth &&
                 m_SyntheticDepthEnabled &&
                 m_SyntheticDepthSequence != 0)
        {
            m_OccupancyMap.clear();
            m_OccupancyMap.integrateDepth(
                m_SyntheticDepth,
                m_SyntheticDepthWidth,
                m_SyntheticDepthHeight,
                1.0f,
                static_cast<float>(m_SyntheticDepthHeight) * 0.5f,
                static_cast<float>(m_SyntheticDepthWidth) * 0.5f,
                static_cast<float>(m_SyntheticDepthWidth) * 0.5f,
                static_cast<float>(m_SyntheticDepthHeight) * 0.5f,
                m_SimulatedRobotPosition,
                m_SimulatedRobotYaw);
            m_LastMappedDepthSequence = m_SyntheticDepthSequence;
        }
        applyTelemetryToRobot();
        updateSimulation(deltaTime);

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
                                    const auto &mappings)
        {
            for (const MotorJointMapping &mapping : mappings)
            {
                const MotorTelemetry *motor = positionFor(mapping.motorId);
                if (motor && motor->error == 0)
                    m_Robot->setJointPosition(
                        mapping.jointName,
                        mapping.modelSign * motor->position);
            }
        };

        applyGroup(kLeftArmMappings);
        applyGroup(kRightArmMappings);
        applyGroup(kLeftLegMappings);
        applyGroup(kRightLegMappings);

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

    void Editor::captureAuthoringKeyframe()
    {
        if (!m_Robot || !m_Robot->isLoaded())
            return;

        m_AuthoringClip.name = m_AuthoringName;
        m_AuthoringClip.duration = m_AuthoringDuration;
        m_AuthoringClip.loop = false;
        m_AuthoringClip.layer = AnimationClip::Layer::Base;

        const UrdfJoint *selectedJoint =
            m_Robot->jointForChildLink(m_SelectedLink);
        for (const auto &[jointName, joint] : m_Robot->data().joints)
        {
            if (joint.type == UrdfJoint::Type::FIXED ||
                (m_AnimateSelectedOnly &&
                 (!selectedJoint || selectedJoint->name != jointName)))
                continue;

            auto trackIt = std::find_if(
                m_AuthoringClip.tracks.begin(),
                m_AuthoringClip.tracks.end(),
                [&jointName](const JointAnimationTrack &track)
                { return track.jointName == jointName; });
            if (trackIt == m_AuthoringClip.tracks.end())
            {
                m_AuthoringClip.tracks.push_back({jointName, {}});
                trackIt = std::prev(m_AuthoringClip.tracks.end());
            }

            auto keyframeIt = std::find_if(
                trackIt->keyframes.begin(),
                trackIt->keyframes.end(),
                [this](const JointKeyframe &keyframe)
                { return keyframe.time == m_AuthoringTime; });
            const JointKeyframe keyframe{
                m_AuthoringTime,
                m_Robot->jointPosition(jointName)};
            if (keyframeIt == trackIt->keyframes.end())
                trackIt->keyframes.push_back(keyframe);
            else
                *keyframeIt = keyframe;
            std::sort(
                trackIt->keyframes.begin(),
                trackIt->keyframes.end(),
                [](const JointKeyframe &left, const JointKeyframe &right)
                { return left.time < right.time; });
        }

        m_AnimationStatus = "Captured authoring pose at " +
                            std::to_string(m_AuthoringTime) + " s";
    }

    bool Editor::saveAuthoringAnimation()
    {
        if (m_AuthoringClip.tracks.empty())
        {
            m_AnimationStatus = "Capture a keyframe before saving";
            return false;
        }

        if (m_AuthoringName.empty() ||
            m_AuthoringName.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
                std::string::npos)
        {
            m_AnimationStatus =
                "Animation name may contain only letters, numbers, '_' and '-'";
            return false;
        }

        const std::filesystem::path path =
            std::filesystem::path("resources") /
            "animations" /
            (m_AuthoringName + ".json");
        std::ofstream file(path);
        if (!file)
        {
            m_AnimationStatus = "Could not save " + path.string();
            return false;
        }

        file << "{\n"
             << "  \"name\": \"" << m_AuthoringName << "\",\n"
             << "  \"duration\": " << m_AuthoringDuration << ",\n"
             << "  \"loop\": false,\n"
             << "  \"tracks\": [\n";
        for (std::size_t trackIndex = 0;
             trackIndex < m_AuthoringClip.tracks.size();
             ++trackIndex)
        {
            const JointAnimationTrack &track =
                m_AuthoringClip.tracks[trackIndex];
            file << "    {\n"
                 << "      \"joint\": \"" << track.jointName << "\",\n"
                 << "      \"keyframes\": [\n";
            for (std::size_t keyframeIndex = 0;
                 keyframeIndex < track.keyframes.size();
                 ++keyframeIndex)
            {
                const JointKeyframe &keyframe =
                    track.keyframes[keyframeIndex];
                file << "        { \"time\": " << keyframe.time
                     << ", \"position\": " << keyframe.position << " }"
                     << (keyframeIndex + 1 == track.keyframes.size() ? "\n" : ",\n");
            }
            file << "      ]\n"
                 << "    }"
                 << (trackIndex + 1 == m_AuthoringClip.tracks.size() ? "\n" : ",\n");
        }
        file << "  ]\n}\n";
        if (!file)
        {
            m_AnimationStatus = "Failed while writing " + path.string();
            return false;
        }

        m_AnimationStatus = "Saved " + path.string();
        return true;
    }

    void Editor::previewAnimation(const AnimationClip &clip)
    {
        if (!m_Robot || !m_Robot->isLoaded())
        {
            m_AnimationStatus = "Cannot preview without a loaded robot";
            return;
        }

        m_AnimationMixer.setClip(clip, *m_Robot);
        m_AnimationDuration = clip.duration;
        m_AnimationTime = 0.0f;
        m_SimulationRunning = true;
        m_AnimationStatus = "Previewing " + clip.name;
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



    void Editor::enterSimulationWorkspace()
    {
        m_SimulationWorkspace = true;
        m_ShowMap = false;
        if (m_TestEnvironmentGenerated)
            return;

        resetTestEnvironment();
    }

    void Editor::resetTestEnvironment()
    {
        m_SimulationMap.clear();
        m_SimulationObstacles.clear();
        m_SimulationPath.clear();
        invalidateSimulationPath();
        m_SimulationPathWaypoint = 1;
        m_SimulationPathReplanTimer = 0.0f;
        const auto addObstacle = [this](
                                     const char *name,
                                     const glm::vec3 &minimum,
                                     const glm::vec3 &maximum)
        {
            m_SimulationObstacles.push_back({name, minimum, maximum});
            m_SimulationMap.addBoxObstacle(minimum, maximum);
        };
        addObstacle("North wall", {-6.0f, 0.0f, -6.0f}, {6.0f, 0.5f, -5.5f});
        addObstacle("South wall", {-6.0f, 0.0f, 5.5f}, {6.0f, 0.5f, 6.0f});
        addObstacle("West wall", {-6.0f, 0.0f, -6.0f}, {-5.5f, 0.5f, 6.0f});
        addObstacle("East wall", {5.5f, 0.0f, -6.0f}, {6.0f, 0.5f, 6.0f});
        addObstacle("West room wall", {-4.5f, 0.0f, -3.5f}, {-4.0f, 1.4f, 1.5f});
        addObstacle("West room divider", {-4.0f, 0.0f, 1.0f}, {-1.5f, 1.0f, 1.5f});
        addObstacle("South corridor wall", {-3.0f, 0.0f, -2.0f}, {1.5f, 1.0f, -1.5f});
        addObstacle("North corridor wall", {-1.5f, 0.0f, 1.5f}, {3.5f, 1.0f, 2.0f});
        addObstacle("Center island", {-0.75f, 0.0f, -0.75f}, {0.75f, 1.8f, 0.75f});
        addObstacle("East room wall", {3.5f, 0.0f, -3.5f}, {4.0f, 1.4f, 1.0f});
        addObstacle("East room divider", {1.5f, 0.0f, 1.0f}, {4.0f, 1.0f, 1.5f});
        addObstacle("Southwest crate", {-3.5f, 0.0f, -4.5f}, {-2.5f, 0.8f, -3.5f});
        addObstacle("Northeast crate", {2.0f, 0.0f, 3.0f}, {3.0f, 0.8f, 4.0f});
        m_SimulatedRobotPosition = {-4.0f, 0.25f, -4.0f};
        m_SimulatedRobotYaw = 0.0f;
        m_SimulationGoal = {4.5f, 0.0f, 4.5f};
        m_HasSimulationGoal = true;
        m_AutonomousNavigation = false;
        m_SimulationAction = "Ready: click Navigate to goal";
        m_SimulationStatus = "Test environment generated";
        m_TestEnvironmentGenerated = true;
    }

    bool Editor::rebuildSimulationPath(
        const PathRequest &request,
        std::vector<glm::vec3> &path)
    {
        constexpr float cellSize = 0.25f;
        constexpr int gridSize = 44;
        constexpr float gridMinimum = -5.5f;
        struct Cell
        {
            int x;
            int z;
            bool operator==(const Cell &other) const { return x == other.x && z == other.z; }
        };
        struct CellHash
        {
            std::size_t operator()(const Cell &cell) const
            {
                return static_cast<std::size_t>((cell.x * 73856093) ^ (cell.z * 19349663));
            }
        };
        const auto positionFor = [](const Cell &cell)
        {
            return glm::vec3(
                gridMinimum + (static_cast<float>(cell.x) + 0.5f) * cellSize,
                0.25f,
                gridMinimum + (static_cast<float>(cell.z) + 0.5f) * cellSize);
        };
        const auto cellFor = [](const glm::vec3 &position)
        {
            return Cell{
                std::clamp(static_cast<int>((position.x - gridMinimum) / cellSize), 0, gridSize - 1),
                std::clamp(static_cast<int>((position.z - gridMinimum) / cellSize), 0, gridSize - 1)};
        };
        const glm::vec3 halfExtents(0.25f, 0.25f, 0.25f);
        const auto intersectsSensorVoxel = [&request, halfExtents](const glm::vec3 &position)
        {
            if (!request.includeSensorMap)
                return false;
            for (const OccupiedVoxel &voxel : request.sensorVoxels)
            {
                const glm::vec3 minimum =
                    glm::vec3(voxel.coordinate) * request.sensorVoxelSize -
                    glm::vec3(request.sensorVoxelSize * 0.5f);
                const glm::vec3 maximum = minimum + request.sensorVoxelSize;
                if (position.x - halfExtents.x <= maximum.x &&
                    position.x + halfExtents.x >= minimum.x &&
                    position.y - halfExtents.y <= maximum.y &&
                    position.y + halfExtents.y >= minimum.y &&
                    position.z - halfExtents.z <= maximum.z &&
                    position.z + halfExtents.z >= minimum.z)
                    return true;
            }
            return false;
        };
        const auto intersectsObstacle = [&request, halfExtents](const glm::vec3 &position)
        {
            for (const SimulationObstacle &obstacle : request.obstacles)
            {
                if (position.x - halfExtents.x <= obstacle.maximum.x &&
                    position.x + halfExtents.x >= obstacle.minimum.x &&
                    position.y - halfExtents.y <= obstacle.maximum.y &&
                    position.y + halfExtents.y >= obstacle.minimum.y &&
                    position.z - halfExtents.z <= obstacle.maximum.z &&
                    position.z + halfExtents.z >= obstacle.minimum.z)
                    return true;
            }
            return false;
        };
        const auto isWalkable = [&intersectsObstacle, &intersectsSensorVoxel](const glm::vec3 &position)
        {
            return !intersectsObstacle(position) && !intersectsSensorVoxel(position);
        };
        const auto nearestWalkable = [&isWalkable, &positionFor](Cell origin)
        {
            for (int radius = 0; radius < 12; ++radius)
                for (int x = origin.x - radius; x <= origin.x + radius; ++x)
                    for (int z = origin.z - radius; z <= origin.z + radius; ++z)
                    {
                        if (x < 0 || x >= gridSize || z < 0 || z >= gridSize ||
                            std::max(std::abs(x - origin.x), std::abs(z - origin.z)) != radius)
                            continue;
                        if (isWalkable(positionFor({x, z})))
                            return Cell{x, z};
                    }
            return origin;
        };

        const Cell start = nearestWalkable(cellFor(request.start));
        const Cell goal = nearestWalkable(cellFor(request.goal));
        if (!isWalkable(positionFor(start)) || !isWalkable(positionFor(goal)))
            return false;

        struct QueueEntry
        {
            float score;
            Cell cell;
            bool operator>(const QueueEntry &other) const { return score > other.score; }
        };
        const auto heuristic = [](const Cell &a, const Cell &b)
        {
            return std::abs(a.x - b.x) + std::abs(a.z - b.z);
        };
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> open;
        std::unordered_map<Cell, float, CellHash> cost;
        std::unordered_map<Cell, Cell, CellHash> parent;
        open.push({0.0f, start});
        cost[start] = 0.0f;
        constexpr std::array<Cell, 8> neighbors{{
            {-1, 0}, {1, 0}, {0, -1}, {0, 1},
            {-1, -1}, {-1, 1}, {1, -1}, {1, 1}}};
        while (!open.empty())
        {
            const Cell current = open.top().cell;
            open.pop();
            if (current == goal)
                break;
            for (const Cell &offset : neighbors)
            {
                const Cell next{current.x + offset.x, current.z + offset.z};
                if (next.x < 0 || next.x >= gridSize || next.z < 0 || next.z >= gridSize ||
                    !isWalkable(positionFor(next)))
                    continue;
                if (offset.x != 0 && offset.z != 0 &&
                    (!isWalkable(positionFor({current.x + offset.x, current.z})) ||
                     !isWalkable(positionFor({current.x, current.z + offset.z}))))
                    continue;
                const float newCost = cost[current] +
                                      (offset.x != 0 && offset.z != 0 ? 1.4142f : 1.0f);
                if (!cost.contains(next) || newCost < cost[next])
                {
                    cost[next] = newCost;
                    parent[next] = current;
                    open.push({newCost + static_cast<float>(heuristic(next, goal)), next});
                }
            }
        }
        if (!cost.contains(goal))
            return false;

        path.clear();
        for (Cell current = goal;; current = parent[current])
        {
            path.push_back(positionFor(current));
            if (current == start)
                break;
        }
        std::reverse(path.begin(), path.end());
        return true;
    }

    void Editor::requestSimulationPath()
    {
        std::lock_guard<std::mutex> lock(m_PathMutex);
        if (m_PathRequestPending || m_PathSearchRunning || m_PathResultReady)
            return;
        m_PathRequest.start = m_SimulatedRobotPosition;
        m_PathRequest.goal = m_SimulationGoal;
        m_PathRequest.generation = ++m_PathRequestGeneration;
        m_PathRequest.obstacles = m_SimulationObstacles;
        m_PathRequest.includeSensorMap = m_ShowSimulationDepth;
        m_PathRequest.sensorVoxelSize = m_OccupancyMap.voxelSize();
        m_PathRequest.sensorVoxels = m_OccupancyMap.voxels();
        m_PathRequestPending = true;
        m_PathResultReady = false;
        m_PathCondition.notify_one();
    }

    void Editor::invalidateSimulationPath()
    {
        std::lock_guard<std::mutex> lock(m_PathMutex);
        ++m_PathRequestGeneration;
        m_PathResultReady = false;
    }

    void Editor::applyCompletedSimulationPath()
    {
        std::lock_guard<std::mutex> lock(m_PathMutex);
        if (!m_PathResultReady)
            return;
        m_PathResultReady = false;
        if (m_PathResultGeneration != m_PathRequestGeneration)
            return;
        if (m_PathResultFound)
        {
            m_SimulationPath = std::move(m_PathResult);
            m_SimulationPathWaypoint = 1;
        }
        else
        {
            m_SimulationPath.clear();
            m_SimulationAction = "No route to goal";
            m_SimulationStatus = "Navigation blocked: no route";
        }
    }

    void Editor::generateSyntheticDepth()
    {
        if (!m_SyntheticDepthEnabled ||
            m_SyntheticDepthWidth <= 0 ||
            m_SyntheticDepthHeight <= 0)
            return;

        m_SyntheticDepth.assign(
            static_cast<std::size_t>(m_SyntheticDepthWidth) *
                static_cast<std::size_t>(m_SyntheticDepthHeight),
            0);
        const glm::vec3 sensorPosition =
            m_SimulatedRobotPosition + glm::vec3(0.0f, 0.35f, 0.0f);
        const float cosYaw = std::cos(m_SimulatedRobotYaw);
        const float sinYaw = std::sin(m_SimulatedRobotYaw);
        for (int y = 0; y < m_SyntheticDepthHeight; ++y)
        {
            const float verticalAngle =
                (0.5f - (static_cast<float>(y) + 0.5f) /
                            static_cast<float>(m_SyntheticDepthHeight)) *
                m_SyntheticDepthVerticalFov;
            for (int x = 0; x < m_SyntheticDepthWidth; ++x)
            {
                const float horizontalAngle =
                    ((static_cast<float>(x) + 0.5f) /
                         static_cast<float>(m_SyntheticDepthWidth) -
                     0.5f) *
                    m_SyntheticDepthHorizontalFov;
                const glm::vec3 localDirection(
                    std::sin(horizontalAngle) * std::cos(verticalAngle),
                    std::sin(verticalAngle),
                    std::cos(horizontalAngle) * std::cos(verticalAngle));
                const glm::vec3 direction(
                    cosYaw * localDirection.x + sinYaw * localDirection.z,
                    localDirection.y,
                    -sinYaw * localDirection.x + cosYaw * localDirection.z);
                float nearestDistance = m_SyntheticDepthRange;
                for (const SimulationObstacle &obstacle : m_SimulationObstacles)
                {
                    float hitDistance = 0.0f;
                    if (intersectRayBox(
                            sensorPosition,
                            direction,
                            obstacle.minimum,
                            obstacle.maximum,
                            hitDistance))
                        nearestDistance = std::min(nearestDistance, hitDistance);
                }
                if (nearestDistance < m_SyntheticDepthRange)
                    m_SyntheticDepth[
                        static_cast<std::size_t>(y) * m_SyntheticDepthWidth + x] =
                        static_cast<std::uint16_t>(
                            std::clamp(nearestDistance * 1000.0f, 1.0f, 65535.0f));
            }
        }
        ++m_SyntheticDepthSequence;
    }

    void Editor::updateSimulation(float deltaTime)
    {
        if (!m_SimulationWorkspace || m_MapNavigationMode)
            return;

        applyCompletedSimulationPath();
        m_SimulationPathReplanTimer -= deltaTime;
        glm::vec3 direction(0.0f);
        if (m_AutonomousNavigation && m_HasSimulationGoal)
        {
            const glm::vec3 toGoal = m_SimulationGoal - m_SimulatedRobotPosition;
            const glm::vec2 planarGoal(toGoal.x, toGoal.z);
            if (glm::length(planarGoal) <= 0.15f)
            {
                m_AutonomousNavigation = false;
                m_SimulationAction = "Goal reached";
                m_SimulationStatus = "Goal reached";
                return;
            }
            const bool shouldReplan =
                m_SimulationPath.empty() ||
                m_SimulationPathReplanTimer <= 0.0f;
            if (shouldReplan)
            {
                requestSimulationPath();
                m_SimulationPathReplanTimer = 0.20f;
            }
            if (m_SimulationPath.empty())
            {
                m_SimulationAction = "Planning route";
                return;
            }
            while (m_SimulationPathWaypoint + 1 < m_SimulationPath.size() &&
                   glm::length(
                       glm::vec2(
                           m_SimulationPath[m_SimulationPathWaypoint].x -
                               m_SimulatedRobotPosition.x,
                           m_SimulationPath[m_SimulationPathWaypoint].z -
                               m_SimulatedRobotPosition.z)) <= 0.12f)
                ++m_SimulationPathWaypoint;
            const glm::vec3 nextWaypoint =
                m_SimulationPathWaypoint < m_SimulationPath.size()
                    ? m_SimulationPath[m_SimulationPathWaypoint]
                    : m_SimulationGoal;
            direction = nextWaypoint - m_SimulatedRobotPosition;
            direction.y = 0.0f;
            m_SimulationAction = "Navigate to goal";
        }
        else
        {
            if (MaraGl::Input::IsKeyDown(GLFW_KEY_LEFT) || MaraGl::Input::IsKeyDown(GLFW_KEY_A))
                direction.x -= 1.0f;
            if (MaraGl::Input::IsKeyDown(GLFW_KEY_RIGHT) || MaraGl::Input::IsKeyDown(GLFW_KEY_D))
                direction.x += 1.0f;
            if (MaraGl::Input::IsKeyDown(GLFW_KEY_UP) || MaraGl::Input::IsKeyDown(GLFW_KEY_W))
                direction.z -= 1.0f;
            if (MaraGl::Input::IsKeyDown(GLFW_KEY_DOWN) || MaraGl::Input::IsKeyDown(GLFW_KEY_S))
                direction.z += 1.0f;
            m_SimulationAction = "Manual drive";
        }
        if (glm::length(direction) <= 0.0f)
        {
            if (!m_AutonomousNavigation)
                m_SimulationAction = "Idle";
            return;
        }

        direction = glm::normalize(direction);
        m_SimulatedRobotYaw = std::atan2(direction.x, direction.z);
        const glm::vec3 candidate =
            m_SimulatedRobotPosition + direction * m_SimulationSpeed * deltaTime;
        const glm::vec3 halfExtents(0.25f, 0.25f, 0.25f);
        const auto isBlocked = [this, halfExtents](const glm::vec3 &position)
        {
            return m_SimulationMap.intersectsBox(
                       position - halfExtents,
                       position + halfExtents) ||
                   (m_ShowSimulationDepth &&
                    m_OccupancyMap.intersectsBox(
                        position - halfExtents,
                        position + halfExtents));
        };
        const auto isBlockedByTestMap = [this, halfExtents](const glm::vec3 &position)
        {
            return m_SimulationMap.intersectsBox(
                position - halfExtents,
                position + halfExtents);
        };
        const auto isBlockedBySensorMap = [this, halfExtents](const glm::vec3 &position)
        {
            return m_ShowSimulationDepth &&
                   m_OccupancyMap.intersectsBox(
                       position - halfExtents,
                       position + halfExtents);
        };
        glm::vec3 selectedCandidate = candidate;
        m_SimulatedRobotYaw = std::atan2(direction.x, direction.z);
        if (isBlocked(selectedCandidate))
        {
            m_SimulationStatus = isBlockedByTestMap(selectedCandidate)
                                     ? "Collision blocked: test obstacle"
                                     : isBlockedBySensorMap(selectedCandidate)
                                           ? "Collision blocked: live depth"
                                           : "Collision blocked";
        }
        else
        {
            m_SimulatedRobotPosition = selectedCandidate;
            m_SimulationStatus = m_AutonomousNavigation
                                     ? "Navigating"
                                     : "Moving";
        }
    }





















    void Editor::shutdown()
    {
        if (!m_Initialized)
            return;

        {
            std::lock_guard<std::mutex> lock(m_PathMutex);
            m_StopPathfindingThread = true;
            m_PathRequestPending = false;
        }
        m_PathCondition.notify_one();
        if (m_PathfindingThread.joinable())
            m_PathfindingThread.join();

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