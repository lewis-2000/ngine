#include "Editor.h"

#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

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

        ImGui::StyleColorsLight();

        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.ItemSpacing = ImVec2(8.0f, 7.0f);
        style.WindowPadding = ImVec2(12.0f, 10.0f);

        ImVec4 *colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.97f, 0.98f, 1.0f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.92f, 0.94f, 0.96f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.78f, 0.84f, 0.90f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.78f, 0.86f, 0.94f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.66f, 0.80f, 0.94f, 1.0f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.52f, 0.70f, 0.90f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.82f, 0.87f, 0.92f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.68f, 0.80f, 0.93f, 1.0f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.12f, 0.42f, 0.72f, 1.0f);
        colors[ImGuiCol_Text] = ImVec4(0.12f, 0.16f, 0.22f, 1.0f);
        colors[ImGuiCol_Border] = ImVec4(0.70f, 0.75f, 0.80f, 0.85f);
        colors[ImGuiCol_Tab] = ImVec4(0.78f, 0.84f, 0.90f, 1.0f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.42f, 0.64f, 0.86f, 1.0f);
        colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.52f, 0.76f, 1.0f);

        io.Fonts->AddFontFromFileTTF(
            "resources/fonts/Roboto-Variable.ttf",
            18.0f);

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
        updateRemoteCameraTexture();
        applyTelemetryToRobot();

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

        setSimulationTime(m_AnimationTime + deltaTime);
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
                [id](const MotorTelemetry &motor) { return motor.id == id; });
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
            return;

        if (m_RemoteCameraTexture == 0)
        {
            glGenTextures(1, &m_RemoteCameraTexture);
            glBindTexture(GL_TEXTURE_2D, m_RemoteCameraTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, m_RemoteCameraTexture);
        }

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
        glBindTexture(GL_TEXTURE_2D, 0);

        m_RemoteCameraSequence = snapshot.cameraSequence;
        m_RemoteCameraWidth = snapshot.cameraWidth;
        m_RemoteCameraHeight = snapshot.cameraHeight;
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
        if (m_LoopAnimation && m_AnimationDuration > 0.0f)
        {
            time = std::fmod(time, m_AnimationDuration);
            if (time < 0.0f)
                time += m_AnimationDuration;
        }
        else
        {
            time = std::clamp(time, 0.0f, m_AnimationDuration);
        }

        m_AnimationTime = time;
        if (m_Robot && m_Robot->isLoaded())
        {
            if (m_AnimateSelectedOnly)
                m_Robot->updateDemoAnimation(m_AnimationTime, m_SelectedLink);
            else
                m_Robot->updateDemoAnimation(m_AnimationTime);
        }
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
        // Split off the left side for the Scene panel.
        //
        // 20% left
        // 80% remaining
        // ------------------------------------------------------------

        ImGuiID leftNode;
        ImGuiID centerNode;

        ImGui::DockBuilderSplitNode(
            mainNode,
            ImGuiDir_Left,
            0.22f,
            &leftNode,
            &centerNode);

        // ------------------------------------------------------------
        // Split the center/right area.
        //
        // 20% right
        // 80% center
        // ------------------------------------------------------------

        ImGuiID rightNode;
        ImGuiID viewportNode;

        ImGui::DockBuilderSplitNode(
            centerNode,
            ImGuiDir_Right,
            0.23f,
            &rightNode,
            &viewportNode);

        // ------------------------------------------------------------
        // Split the bottom of the viewport.
        //
        // 20% bottom
        // 80% viewport
        // ------------------------------------------------------------

        ImGuiID bottomNode;
        ImGuiID finalViewportNode;

        ImGui::DockBuilderSplitNode(
            viewportNode,
            ImGuiDir_Down,
            0.22f,
            &bottomNode,
            &finalViewportNode);

        // ------------------------------------------------------------
        // Split the right side so Inspector and Motion remain visible
        // without relying on a dock tab bar.
        // ------------------------------------------------------------

        ImGuiID inspectorNode;
        ImGuiID motionNode;

        ImGui::DockBuilderSplitNode(
            rightNode,
            ImGuiDir_Down,
            0.48f,
            &motionNode,
            &inspectorNode);

        ImGui::DockBuilderDockWindow(
            "World",
            leftNode);

        ImGui::DockBuilderDockWindow(
            "Inspector",
            inspectorNode);

        ImGui::DockBuilderDockWindow(
            "Motion",
            motionNode);

        ImGui::DockBuilderDockWindow(
            "Viewport",
            finalViewportNode);

        ImGui::DockBuilderDockWindow(
            "Console",
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
            ImGui::MenuItem("Camera preview", nullptr, &m_ShowPosePanel);
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
        ImGui::Begin("Motion", nullptr, ImGuiWindowFlags_NoTitleBar);

        drawPanelHeader("[J]", "JOINT CONTROL", m_SimulationRunning ? "PLAYBACK" : "MANUAL");
        ImGui::TextDisabled("TIMELINE");
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
        {
            resetSimulation();
        }

        ImGui::Checkbox("Loop", &m_LoopAnimation);
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
                    ? "Playing: %s"
                    : "Select a movable link to animate",
                selectedJoint ? selectedJoint->name.c_str() : "");
        }
        const bool timelineChanged = ImGui::SliderFloat(
            "Time",
            &m_AnimationTime,
            0.0f,
            m_AnimationDuration,
            "%.2f s");
        if (timelineChanged)
        {
            m_SimulationRunning = false;
            setSimulationTime(m_AnimationTime);
        }
        ImGui::Text("%.2f / %.2f seconds", m_AnimationTime, m_AnimationDuration);
        ImGui::Separator();
        ImGui::TextDisabled(
            m_SimulationRunning ? "Procedural playback running" : "Manual pose editing");

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

        ImGui::Begin("Robot telemetry", &m_ShowTelemetry, ImGuiWindowFlags_NoTitleBar);
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

        if (!m_TelemetryClient.lastError().empty())
            ImGui::TextColored(
                ImVec4(0.68f, 0.24f, 0.20f, 1.0f),
                "%s",
                m_TelemetryClient.lastError().c_str());

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
                ImVec2(0.0f, 220.0f)))
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
                    motor.id < 10 ? "Head" :
                    motor.id < 20 ? "Left arm" :
                    motor.id < 30 ? "Right arm" :
                    motor.id == 31 ? "Waist" :
                    motor.id < 60 ? "Left leg" : "Right leg";
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
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.09f, 0.12f, 1.0f));
        ImGui::Begin(
            "Camera preview",
            &m_ShowPosePanel,
            ImGuiWindowFlags_NoDocking);

        ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.95f, 1.0f), "[CAM]");
        ImGui::SameLine();
        ImGui::TextUnformatted("CAMERA PREVIEW");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 64.0f);
        ImGui::TextColored(
            ImVec4(0.25f, 0.70f, 0.38f, 1.0f),
            "READY");
        ImGui::Separator();
        const bool remoteCameraAvailable =
            m_RemoteCameraTexture != 0 &&
            m_TelemetryClient.connected() &&
            !m_TelemetryClient.stale();
        ImGui::TextDisabled(
            remoteCameraAvailable
                ? "ROS 2 camera stream (read-only)."
                : "Waiting for the ROS 2 camera stream.");

        const ImVec2 canvasPosition = ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize(
            ImGui::GetContentRegionAvail().x,
            glm::max(220.0f, ImGui::GetContentRegionAvail().y - 44.0f));
        ImGui::InvisibleButton("PoseCanvas", canvasSize);

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            canvasPosition,
            ImVec2(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y),
            IM_COL32(18, 22, 29, 255),
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
                IM_COL32(255, 255, 255, 255));
        }
        else if (m_CameraCapture.isOpen())
        {
            drawList->AddImage(
                static_cast<ImTextureID>(
                    static_cast<uintptr_t>(m_CameraCapture.texture())),
                canvasPosition,
                ImVec2(
                    canvasPosition.x + canvasSize.x,
                    canvasPosition.y + canvasSize.y),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                IM_COL32(255, 255, 255, 255));
        }

        if (!remoteCameraAvailable && !m_CameraCapture.isOpen())
        {
            const char *message = "Camera capture is unavailable";
            const ImVec2 textSize = ImGui::CalcTextSize(message);
            drawList->AddText(
                ImVec2(
                    canvasPosition.x + (canvasSize.x - textSize.x) * 0.5f,
                    canvasPosition.y + (canvasSize.y - textSize.y) * 0.5f),
                IM_COL32(190, 198, 210, 255),
                message);
        }

        ImGui::TextDisabled(
            remoteCameraAvailable
                ? "Live ROS 2 camera feed."
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

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImGui::DestroyContext();

        m_Initialized = false;
    }

}