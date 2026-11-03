#include "Editor.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include "Input.h"

namespace Mara
{
    namespace
    {
        constexpr ImU32 kViewportCardBackground = IM_COL32(245, 247, 250, 246);
        constexpr ImU32 kViewportCardBorder = IM_COL32(145, 158, 180, 230);
        constexpr ImU32 kViewportImageBackground = IM_COL32(224, 228, 235, 255);
        constexpr ImU32 kViewportText = IM_COL32(30, 38, 52, 255);
        constexpr ImU32 kViewportMutedText = IM_COL32(92, 103, 122, 255);
        constexpr ImU32 kTextureTint = IM_COL32(255, 255, 255, 255);
        const ImVec4 kViewportPanelBackground(0.94f, 0.95f, 0.97f, 1.0f);
        const ImVec4 kViewportAccent(0.16f, 0.38f, 0.68f, 1.0f);
        const ImVec4 kHealthyStatus(0.38f, 0.72f, 0.45f, 1.0f);

        void drawPanelHeader(
            const char *icon,
            const char *title,
            const char *status = nullptr)
        {
            ImGui::TextColored(ImVec4(0.16f, 0.38f, 0.68f, 1.0f), "%s", icon);
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
            ImGui::Spacing();
        }

        void drawStatusBadge(const char *label, const ImVec4 &color)
        {
            ImGui::TextColored(color, "%s", label);
        }
    }

    void Editor::setupDockspace()
    {
        ImGuiID dockspaceID = ImGui::GetID("MainDockspace");

        ImGui::DockSpaceOverViewport(
            dockspaceID,
            ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_None);

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

        ImGuiID mainNode = dockspaceID;
        ImGuiID sideNode = 0;
        ImGuiID workNode = 0;
        ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Left, 0.21f,
                                    &sideNode, &workNode);

        ImGuiID inspectorNode = 0;
        ImGuiID centerNode = 0;
        ImGui::DockBuilderSplitNode(workNode, ImGuiDir_Right, 0.23f,
                                    &inspectorNode, &centerNode);

        ImGuiID bottomNode = 0;
        ImGuiID centerNodeWithBottom = 0;
        ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Down, 0.22f,
                                    &bottomNode, &centerNodeWithBottom);

        ImGui::DockBuilderDockWindow("World", sideNode);
        ImGui::DockBuilderDockWindow("Inspector", inspectorNode);
        ImGui::DockBuilderDockWindow("Viewport", centerNodeWithBottom);
        ImGui::DockBuilderDockWindow("Animation", bottomNode);
        ImGui::DockBuilderDockWindow("Robot telemetry", bottomNode);
        ImGui::DockBuilderDockWindow("Console", bottomNode);

        // Select the scene viewport as the initial center document tab.
        // This is used when the default dock layout is first generated.
        if (ImGuiDockNode *centerNodePtr = ImGui::DockBuilderGetNode(centerNodeWithBottom))
            centerNodePtr->SelectedTabId = ImHashStr("Viewport");

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
        if (m_SimulationWorkspace)
        {
            drawSimulationWorkspace();
        }
        else
        {
            drawScene();
            drawViewport();
            drawInspector();
            drawMotion();
            drawConsole();
            drawPosePanel();
            drawTelemetry();
            drawMapPanel();
        }
        if (m_ShowWorkspaceChooser)
        {
            ImGui::OpenPopup("Choose startup workspace");
            if (ImGui::BeginPopupModal(
                    "Choose startup workspace",
                    nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted("Select a workspace to open.");
                ImGui::Spacing();
                if (ImGui::Button("Animation workspace", ImVec2(190.0f, 0.0f)))
                {
                    m_SimulationWorkspace = false;
                    m_ShowWorkspaceChooser = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Simulation workspace", ImVec2(190.0f, 0.0f)))
                {
                    enterSimulationWorkspace();
                    m_ShowWorkspaceChooser = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void Editor::drawToolbar()
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        ImGui::TextColored(ImVec4(0.16f, 0.38f, 0.68f, 1.0f), "NGINE");
        ImGui::SameLine();
        ImGui::TextDisabled("ROBOTICS STUDIO");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New world", "Ctrl+N");
            ImGui::MenuItem("Open world...", "Ctrl+O");
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
            if (ImGui::MenuItem("Animation workspace", nullptr, !m_SimulationWorkspace))
            {
                m_SimulationWorkspace = false;
                m_ShowMap = false;
            }
            if (ImGui::MenuItem("Simulation workspace", nullptr, m_SimulationWorkspace))
                enterSimulationWorkspace();
            ImGui::Separator();
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
            if (ImGui::MenuItem("Depth map (Simulation)", nullptr, false))
                enterSimulationWorkspace();
            ImGui::EndMenu();
        }

        const float statusWidth = 330.0f;
        const float statusStart = ImGui::GetWindowWidth() - statusWidth;
        if (ImGui::GetCursorPosX() < statusStart)
            ImGui::SameLine(statusStart);
        else
            ImGui::SameLine();
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

    void Editor::drawSimulationWorkspace()
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
        ImGui::Begin(
            "Simulation",
            nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
        drawPanelHeader("[S]", "SIMULATION WORKSPACE", "DEPTH MAP + COLLISION");

        const RosTelemetrySnapshot *depthSnapshot = nullptr;
        if (m_TelemetryClient.hasData() &&
            m_TelemetryClient.snapshot().depthSequence != 0)
            depthSnapshot = &m_TelemetryClient.snapshot();
        const auto distanceToObstacle = [this](const SimulationObstacle &obstacle)
        {
            const glm::vec3 closest(
                glm::clamp(m_SimulatedRobotPosition.x, obstacle.minimum.x, obstacle.maximum.x),
                glm::clamp(m_SimulatedRobotPosition.y, obstacle.minimum.y, obstacle.maximum.y),
                glm::clamp(m_SimulatedRobotPosition.z, obstacle.minimum.z, obstacle.maximum.z));
            return glm::length(closest - m_SimulatedRobotPosition);
        };
        float nearestObstacleDistance = 0.0f;
        const SimulationObstacle *nearestObstacle = nullptr;
        for (const SimulationObstacle &obstacle : m_SimulationObstacles)
        {
            const float distance = distanceToObstacle(obstacle);
            if (!nearestObstacle || distance < nearestObstacleDistance)
            {
                nearestObstacle = &obstacle;
                nearestObstacleDistance = distance;
            }
        }
        float nearestLiveDistance = 0.0f;
        bool hasLiveDistance = false;
        for (const OccupiedVoxel &voxel : m_OccupancyMap.voxels())
        {
            const float distance = glm::length(
                glm::vec3(voxel.coordinate) * m_OccupancyMap.voxelSize() -
                m_SimulatedRobotPosition);
            if (!hasLiveDistance || distance < nearestLiveDistance)
            {
                nearestLiveDistance = distance;
                hasLiveDistance = true;
            }
        }
        const float goalDistance = m_HasSimulationGoal
                                       ? glm::length(m_SimulationGoal - m_SimulatedRobotPosition)
                                       : 0.0f;

        const float contentHeight = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild(
            "SimulationControls",
            ImVec2(270.0f, contentHeight),
            true);
        ImGui::TextColored(kViewportAccent, "SIMULATION CONTROLS");
        ImGui::TextDisabled("Local navigation and sensor preview");
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
        {
        ImGui::TextDisabled("Fixed frame");
        const char *fixedFrames[] = {"map", "odom", "base_link"};
        int fixedFrameIndex = m_SimulationFixedFrame == "odom"
                                  ? 1
                                  : m_SimulationFixedFrame == "base_link" ? 2
                                                                            : 0;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##SimulationFixedFrame", &fixedFrameIndex, fixedFrames, 3))
            m_SimulationFixedFrame = fixedFrames[fixedFrameIndex];
        ImGui::Checkbox("RobotModel (URDF)", &m_ShowSimulationRobot);
        ImGui::Checkbox("TF frames (derived)", &m_ShowSimulationTf);
        ImGui::Checkbox("Depth image / points", &m_ShowSimulationDepth);
        ImGui::Checkbox("Occupancy grid", &m_ShowSimulationMap);
        ImGui::Checkbox("Interactive goal marker", &m_ShowSimulationMarker);
        ImGui::TextDisabled(
            "The bridge supplies joints, IMU, and depth; TF is derived from the loaded URDF.");
        }
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Navigation", ImGuiTreeNodeFlags_DefaultOpen))
        {
        ImGui::TextDisabled("Interaction mode");
        if (ImGui::Button(
                m_MapNavigationMode ? "Drive robot" : "Navigate map",
                ImVec2(-1.0f, 0.0f)))
            m_MapNavigationMode = !m_MapNavigationMode;
        ImGui::TextWrapped(
            m_MapNavigationMode
                ? "Map navigation is active. Mouse input controls the 3D camera."
                : "Robot drive is active. WASD and arrow keys move the robot.");
        const float actionWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Reset robot", ImVec2(actionWidth, 0.0f)))
        {
            m_SimulatedRobotPosition = {-4.0f, 0.25f, -4.0f};
            m_SimulatedRobotYaw = 0.0f;
            m_AutonomousNavigation = false;
            m_SimulationAction = "Idle";
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear map", ImVec2(-1.0f, 0.0f)))
        {
            m_OccupancyMap.clear();
            m_LastMappedDepthSequence = 0;
            m_SimulationPath.clear();
            invalidateSimulationPath();
            m_SimulationPathReplanTimer = 0.0f;
        }
        if (ImGui::Button(
                m_AutonomousNavigation ? "Stop navigation" : "Navigate to goal",
                ImVec2(-1.0f, 0.0f)))
        {
            if (m_AutonomousNavigation)
            {
                m_AutonomousNavigation = false;
                m_SimulationAction = "Idle";
            }
            else if (m_HasSimulationGoal)
            {
                m_MapNavigationMode = false;
                m_AutonomousNavigation = true;
                m_SimulationAction = "Navigate to goal";
            }
            else
            {
                m_SimulationStatus = "Place a goal marker first";
            }
        }
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("Speed", &m_SimulationSpeed, 0.25f, 5.0f, "%.2f m/s");
        if (ImGui::Button("Restore test obstacles", ImVec2(-1.0f, 0.0f)))
            resetTestEnvironment();
        }
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Sensor bridge", ImGuiTreeNodeFlags_DefaultOpen))
        {
        ImGui::Checkbox("Use synthetic depth fallback", &m_SyntheticDepthEnabled);
        const bool bridgeLive =
            m_TelemetryClient.connected() && !m_TelemetryClient.stale();
        if (ImGui::Button(
                m_TelemetryClient.connected() ? "Reconnect bridge" : "Connect bridge",
                ImVec2(-1.0f, 0.0f)))
            m_TelemetryClient.connect(
                m_TelemetryClient.host(),
                m_TelemetryClient.port());
        if (bridgeLive)
            ImGui::TextColored(
                ImVec4(0.20f, 0.70f, 0.45f, 1.0f),
                "Depth stream LIVE");
        else
            ImGui::TextColored(
                ImVec4(0.85f, 0.55f, 0.25f, 1.0f),
                "Waiting for depth bridge");
        if (depthSnapshot)
            ImGui::Text(
                "Depth: %d x %d | frame %llu",
                depthSnapshot->depthWidth,
                depthSnapshot->depthHeight,
                static_cast<unsigned long long>(depthSnapshot->depthSequence));
        else if (m_SyntheticDepthEnabled)
            ImGui::Text(
                "Synthetic depth: %d x %d | frame %llu | range %.1f m",
                m_SyntheticDepthWidth,
                m_SyntheticDepthHeight,
                static_cast<unsigned long long>(m_SyntheticDepthSequence),
                m_SyntheticDepthRange);
        }
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen))
        {
        ImGui::Text("Robot position");
        ImGui::Text("(%.2f, %.2f, %.2f)",
                    m_SimulatedRobotPosition.x,
                    m_SimulatedRobotPosition.y,
                    m_SimulatedRobotPosition.z);
        ImGui::Text("Live voxels: %zu", m_OccupancyMap.occupiedCount());
        ImGui::Text("Depth frame: %llu",
                    static_cast<unsigned long long>(m_LastMappedDepthSequence));
        ImGui::TextColored(
            m_SimulationStatus == "Collision blocked"
                ? ImVec4(0.85f, 0.30f, 0.20f, 1.0f)
                : ImVec4(0.20f, 0.70f, 0.45f, 1.0f),
            "%s",
            m_SimulationStatus.c_str());
        ImGui::Text("Action: %s", m_SimulationAction.c_str());
        ImGui::Spacing();
        ImGui::TextColored(kViewportAccent, "PROXIMITY");
        ImGui::Text("Test obstacles: %zu", m_SimulationObstacles.size());
        if (nearestObstacle)
            ImGui::Text(
                "Nearest test: %s (%.2f m)",
                nearestObstacle->name.c_str(),
                nearestObstacleDistance);
        ImGui::Text(
            "Nearest live depth: %s",
            hasLiveDistance ? (std::to_string(nearestLiveDistance) + " m").c_str() : "none");
        ImGui::Text(
            "Goal: %s",
            m_HasSimulationGoal
                ? (std::to_string(goalDistance) + " m").c_str()
                : "not placed");
        if (ImGui::TreeNode("Object proximity details"))
        {
            for (const SimulationObstacle &obstacle : m_SimulationObstacles)
                ImGui::Text(
                    "%s: %.2f m",
                    obstacle.name.c_str(),
                    distanceToObstacle(obstacle));
            ImGui::TreePop();
        }
        ImGui::TextWrapped(
            "3D: right drag orbit, middle drag pan, wheel or +/- zoom. "
            "Click the 2D view to place a goal marker. "
            "Orange voxels are test obstacles; blue voxels are live depth data.");
        }
        ImGui::Spacing();
        ImGui::TextDisabled("3D view");
        const float presetWidth = (ImGui::GetContentRegionAvail().x -
                                   ImGui::GetStyle().ItemSpacing.x * 2.0f) /
                                  3.0f;
        if (ImGui::Button("Perspective", ImVec2(presetWidth, 0.0f)))
        {
            m_MapViewPreset = 0;
            m_MapOrbitYaw = 0.65f;
            m_MapOrbitPitch = 0.55f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Top", ImVec2(presetWidth, 0.0f)))
        {
            m_MapViewPreset = 1;
            m_MapOrbitYaw = 0.0f;
            m_MapOrbitPitch = -1.35f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Side", ImVec2(-1.0f, 0.0f)))
        {
            m_MapViewPreset = 2;
            m_MapOrbitYaw = 1.5708f;
            m_MapOrbitPitch = 0.0f;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("SimulationViews", ImVec2(0.0f, contentHeight), false);
        const float paneWidth = (ImGui::GetContentRegionAvail().x - 12.0f) * 0.5f;
        const float paneHeight = ImGui::GetContentRegionAvail().y;

        ImGui::BeginChild(
            "Map3DPane",
            ImVec2(paneWidth, paneHeight),
            true);
        ImGui::TextColored(kViewportAccent, "3D VISUALIZATION");
        ImGui::SameLine();
        ImGui::TextDisabled("polygon map");
        const ImVec2 viewMin = ImGui::GetCursorScreenPos();
        const ImVec2 viewSize(
            ImGui::GetContentRegionAvail().x,
            glm::max(240.0f, ImGui::GetContentRegionAvail().y));
        ImGui::InvisibleButton("Map3DCanvas", viewSize);
        const ImVec2 viewMax(viewMin.x + viewSize.x, viewMin.y + viewSize.y);
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(viewMin, viewMax, IM_COL32(14, 22, 32, 255));
        const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        if (ImGui::IsItemActive() || ImGui::IsItemHovered())
        {
            if (m_MapNavigationMode && ImGui::GetIO().MouseDown[1])
            {
                m_MapOrbitYaw += mouseDelta.x * 0.008f;
                m_MapOrbitPitch = glm::clamp(
                    m_MapOrbitPitch + mouseDelta.y * 0.008f,
                    -1.35f,
                    1.35f);
            }
            if (m_MapNavigationMode && ImGui::GetIO().MouseDown[2])
            {
                m_MapPan.x -= mouseDelta.x * 0.015f / m_MapZoom;
                m_MapPan.y += mouseDelta.y * 0.015f / m_MapZoom;
            }
            if (m_MapNavigationMode)
                m_MapZoom = glm::clamp(
                    m_MapZoom + ImGui::GetIO().MouseWheel * 0.12f,
                    0.35f,
                    4.0f);
            if (m_MapNavigationMode &&
                (MaraGl::Input::IsKeyDown(GLFW_KEY_EQUAL) ||
                 MaraGl::Input::IsKeyDown(GLFW_KEY_KP_ADD)))
                m_MapZoom = glm::min(4.0f, m_MapZoom + 0.02f);
            if (m_MapNavigationMode &&
                (MaraGl::Input::IsKeyDown(GLFW_KEY_MINUS) ||
                 MaraGl::Input::IsKeyDown(GLFW_KEY_KP_SUBTRACT)))
                m_MapZoom = glm::max(0.35f, m_MapZoom - 0.02f);
        }
        const ImVec2 viewCenter(
            viewMin.x + viewSize.x * 0.5f + m_MapPan.x * 20.0f,
            viewMin.y + viewSize.y * 0.52f + m_MapPan.y * 20.0f);
        const float projectionScale = glm::min(viewSize.x, viewSize.y) * 0.80f * m_MapZoom;
        const float cosYaw = std::cos(m_MapOrbitYaw);
        const float sinYaw = std::sin(m_MapOrbitYaw);
        const float cosPitch = std::cos(m_MapOrbitPitch);
        const float sinPitch = std::sin(m_MapOrbitPitch);
        const auto project3d = [this,
                                viewCenter,
                                projectionScale,
                                cosYaw,
                                sinYaw,
                                cosPitch,
                                sinPitch](const glm::vec3 &position)
        {
            const glm::vec3 translated = position - glm::vec3(m_MapPan.x, 0.0f, m_MapPan.y);
            if (m_MapViewPreset == 1)
                return ImVec2(
                    viewCenter.x + translated.x * projectionScale * 0.08f,
                    viewCenter.y + translated.z * projectionScale * 0.08f);
            if (m_MapViewPreset == 2)
                return ImVec2(
                    viewCenter.x + translated.z * projectionScale * 0.08f,
                    viewCenter.y - translated.y * projectionScale * 0.08f);
            const float yawX = translated.x * cosYaw - translated.z * sinYaw;
            const float yawZ = translated.x * sinYaw + translated.z * cosYaw + 11.0f;
            const float cameraY = translated.y * cosPitch - yawZ * sinPitch;
            const float depth = translated.y * sinPitch + yawZ * cosPitch;
            const float safeDepth = glm::max(1.0f, depth);
            return ImVec2(
                viewCenter.x + yawX * projectionScale / safeDepth,
                viewCenter.y - cameraY * projectionScale / safeDepth);
        };
        const auto drawPolygon = [drawList, &viewMin, &viewMax](
                                     const ImVec2 &point,
                                     float radius,
                                     ImU32 color)
        {
            if (point.x < viewMin.x || point.x > viewMax.x ||
                point.y < viewMin.y || point.y > viewMax.y)
                return;
            const ImVec2 polygon[6] = {
                {point.x - radius, point.y},
                {point.x - radius * 0.5f, point.y - radius},
                {point.x + radius * 0.5f, point.y - radius},
                {point.x + radius, point.y},
                {point.x + radius * 0.5f, point.y + radius},
                {point.x - radius * 0.5f, point.y + radius}};
            drawList->AddConvexPolyFilled(polygon, 6, color);
        };
        constexpr std::size_t kMaxVisibleSimulationVoxels = 12000;
        constexpr std::size_t kMaxVisibleLiveVoxels = 12000;
        std::size_t simulationVoxelCount = 0;
        if (m_ShowSimulationMap)
        {
            for (const OccupiedVoxel &voxel : m_SimulationMap.voxels())
            {
                if (simulationVoxelCount++ >= kMaxVisibleSimulationVoxels)
                    break;
                drawPolygon(
                    project3d(glm::vec3(voxel.coordinate) * m_SimulationMap.voxelSize()),
                    3.5f,
                    IM_COL32(245, 125, 55, 235));
            }
        }
        std::size_t liveVoxelCount = 0;
        if (m_ShowSimulationDepth)
        {
            for (const OccupiedVoxel &voxel : m_OccupancyMap.voxels())
            {
                if (liveVoxelCount++ >= kMaxVisibleLiveVoxels)
                    break;
                drawPolygon(
                    project3d(glm::vec3(voxel.coordinate) * m_OccupancyMap.voxelSize()),
                    3.5f,
                    IM_COL32(80, 175, 255, 235));
            }
        }
        const glm::vec3 scannerPosition = m_SimulatedRobotPosition;
        const ImVec2 sensorPoint = project3d(scannerPosition);
        constexpr float kScannerHalfAngle = 1.10f;
        constexpr int kScannerRayCount = 13;
        float scannerRange = 2.0f;
        if (depthSnapshot && !m_TelemetryClient.stale())
        {
            std::uint16_t nearestDepth = 0;
            for (const std::uint16_t depth : depthSnapshot->depth)
            {
                if (depth == 0)
                    continue;
                if (nearestDepth == 0 || depth < nearestDepth)
                    nearestDepth = depth;
            }
            if (nearestDepth != 0)
                scannerRange = glm::clamp(
                    static_cast<float>(nearestDepth) * 0.001f,
                    0.5f,
                    6.0f);
        }
        ImVec2 previousScanPoint = sensorPoint;
        if (m_ShowSimulationDepth)
        {
            for (int ray = 0; ray < kScannerRayCount; ++ray)
            {
                const float angle =
                    -kScannerHalfAngle +
                    (2.0f * kScannerHalfAngle * static_cast<float>(ray)) /
                        static_cast<float>(kScannerRayCount - 1);
                const float worldAngle = m_SimulatedRobotYaw + angle;
                const ImVec2 scanPoint = project3d({scannerPosition.x + scannerRange * std::sin(worldAngle),
                                                    scannerPosition.y,
                                                    scannerPosition.z + scannerRange * std::cos(worldAngle)});
                drawList->AddLine(
                    sensorPoint,
                    scanPoint,
                    IM_COL32(255, 220, 80, ray == 0 || ray == kScannerRayCount - 1 ? 230 : 85),
                    ray == 0 || ray == kScannerRayCount - 1 ? 2.0f : 1.0f);
                if (ray > 0)
                    drawList->AddLine(
                        previousScanPoint,
                        scanPoint,
                        IM_COL32(255, 220, 80, 120),
                        1.0f);
                previousScanPoint = scanPoint;
            }
            drawList->AddCircleFilled(sensorPoint, 6.0f, IM_COL32(255, 220, 80, 255));
        }
        if (m_ShowSimulationRobot)
        {
            const ImVec2 robotPoint = project3d(m_SimulatedRobotPosition);
            drawList->AddCircleFilled(robotPoint, 9.0f, IM_COL32(75, 235, 170, 255));
            const ImVec2 headingPoint = project3d(
                {m_SimulatedRobotPosition.x + std::sin(m_SimulatedRobotYaw) * 0.7f,
                 m_SimulatedRobotPosition.y,
                 m_SimulatedRobotPosition.z + std::cos(m_SimulatedRobotYaw) * 0.7f});
            drawList->AddLine(robotPoint, headingPoint, IM_COL32(245, 245, 245, 255), 2.0f);
        }
        if (m_ShowSimulationTf && m_Robot && m_Robot->isLoaded())
        {
            drawList->AddText(
                ImVec2(viewMin.x + 10.0f, viewMin.y + 30.0f),
                IM_COL32(170, 205, 235, 255),
                "TF: map -> odom -> base_link -> camera_head_link");
        }
        if (m_ShowSimulationMarker && m_HasSimulationGoal)
        {
            const ImVec2 goalPoint = project3d(m_SimulationGoal);
            drawList->AddCircle(goalPoint, 10.0f, IM_COL32(255, 90, 90, 255), 4, 2.0f);
            drawList->AddLine(
                ImVec2(goalPoint.x - 7.0f, goalPoint.y),
                ImVec2(goalPoint.x + 7.0f, goalPoint.y),
                IM_COL32(255, 90, 90, 255),
                2.0f);
            drawList->AddLine(
                ImVec2(goalPoint.x, goalPoint.y - 7.0f),
                ImVec2(goalPoint.x, goalPoint.y + 7.0f),
                IM_COL32(255, 90, 90, 255),
                2.0f);
        }
        if (m_SimulationPath.size() > 1)
        {
            for (std::size_t index = 1; index < m_SimulationPath.size(); ++index)
                drawList->AddLine(
                    project3d(m_SimulationPath[index - 1]),
                    project3d(m_SimulationPath[index]),
                    IM_COL32(255, 220, 80, 210),
                    2.0f);
        }
        drawList->AddText(
            ImVec2(viewMin.x + 10.0f, viewMin.y + 10.0f),
            IM_COL32(220, 230, 240, 255),
            "RMB orbit | MMB pan | wheel zoom | yellow fan: head scanner FOV");
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild(
            "Map2DPane",
            ImVec2(0.0f, paneHeight),
            true,
            ImGuiWindowFlags_NoScrollbar);
        ImGui::TextColored(kViewportAccent, "2D TOP VIEW");
        ImGui::SameLine();
        ImGui::TextDisabled("collision plane");
        const ImVec2 topMin = ImGui::GetCursorScreenPos();
        const ImVec2 topSize = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton(
            "Map2DCanvas",
            ImVec2(glm::max(1.0f, topSize.x), glm::max(1.0f, topSize.y)));
        const ImVec2 topMax(
            topMin.x + topSize.x,
            topMin.y + topSize.y);
        ImDrawList *topDrawList = ImGui::GetWindowDrawList();
        topDrawList->AddRectFilled(topMin, topMax, IM_COL32(14, 22, 32, 255));
        const ImVec2 topCenter(
            topMin.x + topSize.x * 0.5f,
            topMin.y + topSize.y * 0.5f);
        const float mapExtent = 7.0f;
        const float topScale = glm::max(
            1.0f,
            glm::min(topSize.x, topSize.y) / (mapExtent * 2.0f));
        if (m_ShowSimulationMarker && ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            const ImVec2 click = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
            m_SimulationGoal.x = (click.x - topCenter.x) / topScale;
            m_SimulationGoal.z = (click.y - topCenter.y) / topScale;
            m_SimulationGoal.y = 0.0f;
            m_HasSimulationGoal = true;
            m_SimulationPath.clear();
            invalidateSimulationPath();
            m_SimulationPathReplanTimer = 0.0f;
            m_SimulationStatus = "Goal marker placed";
        }
        const auto project2d = [topCenter, topScale](const glm::vec3 &position)
        {
            return ImVec2(
                topCenter.x + position.x * topScale,
                topCenter.y + position.z * topScale);
        };
        const auto drawTopPolygon = [topDrawList, &topMin, &topMax](
                                        const ImVec2 &point,
                                        ImU32 color)
        {
            if (point.x < topMin.x || point.x > topMax.x ||
                point.y < topMin.y || point.y > topMax.y)
                return;
            topDrawList->AddRectFilled(
                ImVec2(point.x - 2.0f, point.y - 2.0f),
                ImVec2(point.x + 2.0f, point.y + 2.0f),
                color);
        };
        std::size_t topSimulationVoxelCount = 0;
        for (const OccupiedVoxel &voxel : m_SimulationMap.voxels())
        {
            if (topSimulationVoxelCount++ >= kMaxVisibleSimulationVoxels)
                break;
            drawTopPolygon(
                project2d(glm::vec3(voxel.coordinate) * m_SimulationMap.voxelSize()),
                IM_COL32(255, 105, 145, 235));
        }
        std::size_t topLiveVoxelCount = 0;
        if (m_ShowSimulationDepth)
        {
            for (const OccupiedVoxel &voxel : m_OccupancyMap.voxels())
            {
                if (topLiveVoxelCount++ >= kMaxVisibleLiveVoxels)
                    break;
                drawTopPolygon(
                    project2d(glm::vec3(voxel.coordinate) * m_OccupancyMap.voxelSize()),
                    IM_COL32(75, 235, 170, 235));
            }
        }
        if (m_ShowSimulationMap)
        {
            for (const SimulationObstacle &obstacle : m_SimulationObstacles)
            {
                const glm::vec3 center =
                    (obstacle.minimum + obstacle.maximum) * 0.5f;
                const ImVec2 label = project2d(center);
                if (label.x >= topMin.x && label.x <= topMax.x &&
                    label.y >= topMin.y && label.y <= topMax.y)
                    topDrawList->AddText(
                        ImVec2(label.x + 4.0f, label.y - 7.0f),
                        IM_COL32(255, 220, 180, 220),
                        obstacle.name.c_str());
            }
        }
        topDrawList->AddRect(
            ImVec2(topCenter.x - 6.0f * topScale, topCenter.y - 6.0f * topScale),
            ImVec2(topCenter.x + 6.0f * topScale, topCenter.y + 6.0f * topScale),
            IM_COL32(90, 105, 125, 180));
        topDrawList->AddLine(
            ImVec2(topCenter.x - mapExtent * topScale, topCenter.y),
            ImVec2(topCenter.x + mapExtent * topScale, topCenter.y),
            IM_COL32(90, 105, 125, 120));
        topDrawList->AddLine(
            ImVec2(topCenter.x, topCenter.y - mapExtent * topScale),
            ImVec2(topCenter.x, topCenter.y + mapExtent * topScale),
            IM_COL32(90, 105, 125, 120));
        const ImVec2 robot(
            topCenter.x + m_SimulatedRobotPosition.x * topScale,
            topCenter.y + m_SimulatedRobotPosition.z * topScale);
        if (m_ShowSimulationRobot)
        {
            topDrawList->AddCircleFilled(robot, 9.0f, IM_COL32(70, 220, 150, 255));
            topDrawList->AddLine(
                robot,
                project2d({m_SimulatedRobotPosition.x + std::sin(m_SimulatedRobotYaw) * 0.7f,
                           m_SimulatedRobotPosition.y,
                           m_SimulatedRobotPosition.z + std::cos(m_SimulatedRobotYaw) * 0.7f}),
                IM_COL32(245, 245, 245, 255),
                2.0f);
        }
        if (m_ShowSimulationMarker && m_HasSimulationGoal)
        {
            const ImVec2 goal(
                topCenter.x + m_SimulationGoal.x * topScale,
                topCenter.y + m_SimulationGoal.z * topScale);
            topDrawList->AddCircle(goal, 8.0f, IM_COL32(255, 90, 90, 255), 4, 2.0f);
            topDrawList->AddLine(
                ImVec2(goal.x - 6.0f, goal.y),
                ImVec2(goal.x + 6.0f, goal.y),
                IM_COL32(255, 90, 90, 255),
                2.0f);
            topDrawList->AddLine(
                ImVec2(goal.x, goal.y - 6.0f),
                ImVec2(goal.x, goal.y + 6.0f),
                IM_COL32(255, 90, 90, 255),
                2.0f);
        }
        if (m_SimulationPath.size() > 1)
        {
            for (std::size_t index = 1; index < m_SimulationPath.size(); ++index)
                topDrawList->AddLine(
                    project2d(m_SimulationPath[index - 1]),
                    project2d(m_SimulationPath[index]),
                    IM_COL32(255, 220, 80, 230),
                    2.0f);
        }
        const glm::vec3 topScannerPosition = m_SimulatedRobotPosition;
        const ImVec2 scannerOrigin = project2d(topScannerPosition);
        ImVec2 previousTopScanPoint = scannerOrigin;
        for (int ray = 0; ray < 13; ++ray)
        {
            const float angle =
                -1.10f +
                (2.20f * static_cast<float>(ray)) / 12.0f;
            const float worldAngle = m_SimulatedRobotYaw + angle;
            const ImVec2 scanPoint = project2d({topScannerPosition.x + 2.0f * std::sin(worldAngle),
                                                topScannerPosition.y,
                                                topScannerPosition.z + 2.0f * std::cos(worldAngle)});
            topDrawList->AddLine(
                scannerOrigin,
                scanPoint,
                IM_COL32(255, 220, 80, ray == 0 || ray == 12 ? 230 : 85),
                ray == 0 || ray == 12 ? 2.0f : 1.0f);
            if (ray > 0)
                topDrawList->AddLine(
                    previousTopScanPoint,
                    scanPoint,
                    IM_COL32(255, 220, 80, 120),
                    1.0f);
            previousTopScanPoint = scanPoint;
        }
        topDrawList->AddCircleFilled(
            scannerOrigin,
            5.0f,
            IM_COL32(255, 220, 80, 255));
        drawList->AddText(
            ImVec2(topMin.x + 10.0f, topMin.y + 10.0f),
            IM_COL32(220, 230, 240, 255),
            "pink obstacles | green live scan | blue robot");
        char mapCountLabel[64] = {};
        std::snprintf(
            mapCountLabel,
            sizeof(mapCountLabel),
            "Map data: %zu voxels",
            m_SimulationMap.occupiedCount() + m_OccupancyMap.occupiedCount());
        drawList->AddText(
            ImVec2(topMin.x + 10.0f, topMax.y - 24.0f),
            IM_COL32(170, 185, 200, 255),
            mapCountLabel);
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::End();
    }

    void Editor::drawScene()
    {
        ImGui::Begin("World");

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 20.0f);

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

        ImGui::PopStyleVar(3);
        ImGui::End();
    }

    void Editor::drawViewport()
    {
        // A distinct cool, deep surface frames the viewport as the primary
        // workspace, while the bright cyan rail marks it as the active tool.
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.93f, 0.94f, 0.96f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.52f, 0.68f, 0.95f));
        ImGui::Begin("Viewport");

        ImDrawList *windowDrawList = ImGui::GetWindowDrawList();
        const ImVec2 windowMin = ImGui::GetWindowPos();
        const float titleHeight = ImGui::GetFrameHeight();
        windowDrawList->AddRectFilled(
            ImVec2(windowMin.x, windowMin.y + titleHeight),
            ImVec2(windowMin.x + 3.0f, windowMin.y + ImGui::GetWindowHeight()),
            IM_COL32(35, 220, 235, 255));

        ImGui::PushFont(m_FontHeading);
        ImGui::TextColored(ImVec4(0.16f, 0.38f, 0.68f, 1.0f), "SCENE VIEWPORT");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextDisabled("/  PERSPECTIVE");
        ImGui::SameLine();
        ImGui::TextDisabled("/  TIENKUNG ROBOT");

        // Keep navigation controls grouped together on a dedicated toolbar row.
        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.82f, 0.85f, 0.90f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.78f, 0.91f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.62f, 0.84f, 1.0f));
        if (ImGui::Button("Front view"))
            m_CameraResetRequested = true;
        ImGui::SameLine();
        if (ImGui::Button(m_ShowGrid ? "Grid  ON" : "Grid  OFF"))
            m_ShowGrid = !m_ShowGrid;
        ImGui::SameLine();
        if (ImGui::Button(m_ShowSensors ? "Sensors  ON" : "Sensors  OFF"))
            m_ShowSensors = !m_ShowSensors;
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(8.0f, 1.0f));
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, m_SimulationRunning
                                                   ? ImVec4(0.42f, 0.25f, 0.08f, 1.0f)
                                                   : ImVec4(0.18f, 0.34f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_SimulationRunning
                                                          ? ImVec4(0.58f, 0.34f, 0.10f, 1.0f)
                                                          : ImVec4(0.28f, 0.46f, 0.70f, 1.0f));
        if (ImGui::Button(m_SimulationRunning ? "||  Pause simulation" : ">  Run simulation"))
            m_SimulationRunning = !m_SimulationRunning;
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Reset camera"))
            m_CameraResetRequested = true;
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::Separator();
        ImGui::Spacing();

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        int width = static_cast<int>(viewportSize.x);
        int height = static_cast<int>(viewportSize.y);

        if (width > 0 && height > 0)
        {
            m_RequestedSceneWidth = width;
            m_RequestedSceneHeight = height;

            if (m_ShowMap)
            {
                ImGui::BeginChild("MapModeNotice", viewportSize, true);
                ImGui::TextColored(kViewportAccent, "MAP MODE");
                ImGui::TextDisabled(
                    "Detailed robot rendering is paused while the depth map is active.");
                ImGui::EndChild();
                m_ViewportHovered = false;
            }
            else
            {
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
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
    }

    void Editor::drawMapPanel()
    {
        if (!m_ShowMap)
            return;

        ImGui::SetNextWindowSize(ImVec2(560.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Depth map", &m_ShowMap);
        drawPanelHeader("[M]", "DEPTH MAP", m_MapTopView ? "TOP VIEW" : "3D VIEW");
        if (ImGui::Button(m_MapTopView ? "Switch to 3D" : "Switch to top view"))
            m_MapTopView = !m_MapTopView;
        ImGui::SameLine();
        if (ImGui::Button("Clear map"))
            m_OccupancyMap.clear();
        ImGui::SameLine();
        ImGui::Text("%zu occupied voxels", m_OccupancyMap.occupiedCount());

        float voxelSize = m_OccupancyMap.voxelSize();
        if (ImGui::SliderFloat("Voxel size", &voxelSize, 0.02f, 0.25f, "%.2f m"))
            m_OccupancyMap.setVoxelSize(voxelSize);
        ImGui::InputText("Map file", m_MapPath, sizeof(m_MapPath));
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            const std::filesystem::path path(m_MapPath);
            if (!path.parent_path().empty())
                std::filesystem::create_directories(path.parent_path());
            if (!m_OccupancyMap.save(path))
                m_AnimationStatus = "Map save failed: " + path.string();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load") && !m_OccupancyMap.load(m_MapPath))
            m_AnimationStatus = "Map load failed: " + std::string(m_MapPath);

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize(
            ImGui::GetContentRegionAvail().x,
            glm::max(280.0f, ImGui::GetContentRegionAvail().y));
        ImGui::InvisibleButton("MapCanvas", canvasSize);
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 27, 38, 255));
        const float scale = glm::min(canvasSize.x, canvasSize.y) * 0.12f;
        const ImVec2 center(
            canvasMin.x + canvasSize.x * 0.5f,
            canvasMin.y + canvasSize.y * 0.55f);
        for (const OccupiedVoxel &voxel : m_OccupancyMap.voxels())
        {
            const float x = static_cast<float>(voxel.coordinate.x) * m_OccupancyMap.voxelSize();
            const float y = static_cast<float>(voxel.coordinate.y) * m_OccupancyMap.voxelSize();
            const float z = static_cast<float>(voxel.coordinate.z) * m_OccupancyMap.voxelSize();
            ImVec2 point;
            if (m_MapTopView)
                point = ImVec2(center.x + x * scale, center.y + z * scale);
            else
                point = ImVec2(
                    center.x + (x - z) * scale * 0.7f,
                    center.y + (x + z) * scale * 0.35f - y * scale);
            if (point.x >= canvasMin.x && point.x <= canvasMax.x &&
                point.y >= canvasMin.y && point.y <= canvasMax.y)
                drawList->AddRectFilled(
                    ImVec2(point.x - 2.0f, point.y - 2.0f),
                    ImVec2(point.x + 2.0f, point.y + 2.0f),
                    IM_COL32(45, 205, 220, 220));
        }
        drawList->AddText(
            ImVec2(canvasMin.x + 10.0f, canvasMin.y + 10.0f),
            IM_COL32(210, 220, 232, 255),
            m_MapTopView ? "Top view (X/Z)" : "Projected 3D view");
        drawList->AddCircleFilled(center, 7.0f, IM_COL32(245, 170, 55, 255));
        drawList->AddText(
            ImVec2(center.x + 10.0f, center.y - 7.0f),
            IM_COL32(245, 220, 170, 255),
            "robot proxy");
        ImGui::End();
    }

    void Editor::drawInspector()
    {
        ImGui::Begin("Inspector");

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

        if (!ImGui::Begin(
                "Animation",
                &m_ShowAnimationPanel))
        {
            ImGui::End();
            return;
        }

        drawPanelHeader(
            "[J]",
            "MOTION",
            m_SimulationRunning ? "PLAYBACK" : "MANUAL EDITING");

        refreshDemoAnimation();

        if (ImGui::CollapsingHeader(
                "Loaded animations",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto &clips = m_AnimationLibrary.catalog().clips();
            if (clips.empty())
            {
                ImGui::TextDisabled("No JSON animations found");
            }
            else
            {
                for (const AnimationClip &clip : clips)
                {
                    ImGui::PushID(clip.name.c_str());
                    ImGui::Text("%s", clip.name.c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled(
                        "%.2f s | %zu tracks",
                        clip.duration,
                        clip.tracks.size());
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70.0f);
                    if (ImGui::SmallButton("Preview"))
                        previewAnimation(clip);
                    ImGui::PopID();
                }
            }
        }

        if (ImGui::CollapsingHeader(
                "Animation authoring",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            char animationName[128] = {};
            std::snprintf(
                animationName,
                sizeof(animationName),
                "%s",
                m_AuthoringName.c_str());
            if (ImGui::InputText(
                    "Name",
                    animationName,
                    sizeof(animationName)))
                m_AuthoringName = animationName;

            ImGui::DragFloat(
                "Duration",
                &m_AuthoringDuration,
                0.05f,
                0.1f,
                120.0f,
                "%.2f s");
            m_AuthoringTime = glm::clamp(
                m_AuthoringTime,
                0.0f,
                m_AuthoringDuration);
            ImGui::SliderFloat(
                "Keyframe time",
                &m_AuthoringTime,
                0.0f,
                m_AuthoringDuration,
                "%.2f s");
            ImGui::TextDisabled(
                "%zu tracks captured",
                m_AuthoringClip.tracks.size());

            if (ImGui::Button("Capture pose"))
                captureAuthoringKeyframe();
            ImGui::SameLine();
            if (ImGui::Button("Preview"))
            {
                if (!m_AuthoringClip.tracks.empty() &&
                    m_Robot &&
                    m_Robot->isLoaded())
                {
                    m_AnimationMixer.setClip(m_AuthoringClip, *m_Robot);
                    m_AnimationTime = 0.0f;
                    m_SimulationRunning = true;
                    m_AnimationStatus = "Previewing " + m_AuthoringName;
                }
                else
                {
                    m_AnimationStatus =
                        "Capture at least one pose before previewing";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Save JSON"))
                saveAuthoringAnimation();
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                m_AuthoringClip.tracks.clear();
                m_AnimationStatus = "Authoring keyframes cleared";
            }
        }

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

        ImGui::Begin("Console");

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
        ImGui::Spacing();
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

        if (ImGui::Button("Open motor telemetry"))
            m_ShowMotorTelemetryPanel = true;
        ImGui::SameLine();
        ImGui::TextDisabled("Open the full motor table in a floating panel.");
        ImGui::End();

        if (!m_ShowMotorTelemetryPanel)
            return;

        ImGui::SetNextWindowSize(ImVec2(760.0f, 420.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin(
            "Motor telemetry",
            &m_ShowMotorTelemetryPanel,
            ImGuiWindowFlags_NoDocking);
        drawPanelHeader(
            "[MOT]",
            "MOTOR TELEMETRY",
            live ? "LIVE DATA" : "WAITING");

        if (ImGui::BeginTable(
                "motorTelemetryFloating",
                7,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY,
                ImVec2(0.0f, ImGui::GetContentRegionAvail().y - 28.0f)))
        {
            const char *columns[] = {"Motor", "Group", "Position", "Speed", "Current", "Temp", "State"};
            ImGui::TableSetupColumn(columns[0], ImGuiTableColumnFlags_WidthFixed, 78.0f);
            ImGui::TableSetupColumn(columns[1], ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn(columns[2], ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn(columns[3], ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn(columns[4], ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn(columns[5], ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn(columns[6], ImGuiTableColumnFlags_WidthFixed, 92.0f);
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
            "Raw read-only bridge values. Model playback applies mirrored joint direction conversion.");
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
}
