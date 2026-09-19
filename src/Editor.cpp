#include "Editor.h"

#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

namespace Mara
{

    Editor::~Editor()
    {
        shutdown();
    }

    void Editor::initialize(GLFWwindow *window, const Robot *robot)
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

        ImGui::StyleColorsDark();

        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 3.0f;
        style.FrameRounding = 2.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.WindowPadding = ImVec2(10.0f, 10.0f);

        ImVec4 *colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.085f, 0.095f, 1.0f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.065f, 0.075f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.16f, 0.19f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.12f, 0.22f, 0.27f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.30f, 0.35f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.12f, 0.17f, 0.19f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.17f, 0.30f, 0.34f, 1.0f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.27f, 0.75f, 0.76f, 1.0f);

        io.Fonts->AddFontFromFileTTF(
            "resources/fonts/Roboto-Variable.ttf",
            18.0f);

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        m_Initialized = true;
    }

    void Editor::setupDockspace()
    {
        ImGuiID dockspaceID = ImGui::GetID("MainDockspace");

        ImGui::DockSpaceOverViewport(
            dockspaceID,
            ImGui::GetMainViewport());

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
            0.20f,
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
            0.20f,
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
            0.20f,
            &bottomNode,
            &finalViewportNode);

        // ------------------------------------------------------------
        // Put each editor window into its dock node.
        // ------------------------------------------------------------

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
        drawConsole();
    }

    void Editor::beginSceneRender(int width, int height)
    {
        if (!m_Initialized)
            return;

        m_SceneFramebuffer.resize(width, height);
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
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void Editor::drawScene()
    {
        ImGui::Begin("World");

        ImGui::TextUnformatted("WORLD");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22.0f);
        ImGui::SmallButton("+");
        ImGui::Separator();
        ImGui::TextDisabled("Entities");

        if (ImGui::TreeNodeEx("world", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "World"))
        {
            const char *robotName = m_Robot && m_Robot->isLoaded()
                                        ? m_Robot->name().c_str()
                                        : "No robot loaded";

            if (ImGui::TreeNodeEx("robot", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", robotName))
            {
                if (m_Robot && m_Robot->isLoaded())
                {
                    for (const auto &[linkName, link] : m_Robot->data().links)
                    {
                        ImGui::TreeNodeEx(
                            linkName.c_str(),
                            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
                            "%s",
                            linkName.c_str());
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreeNodeEx("sun", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "sun");
            ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Simulation");
        ImGui::Text("Physics");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48.0f);
        ImGui::TextColored(ImVec4(0.30f, 0.78f, 0.70f, 1.0f), "Ready");

        ImGui::End();
    }

    void Editor::drawViewport()
    {
        ImGui::Begin("Viewport");

        ImGui::TextUnformatted("PERSPECTIVE");
        ImGui::SameLine();
        ImGui::TextDisabled("|  robot_alpha  |  60 FPS");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 170.0f);
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
            ImGui::Image(
                static_cast<ImTextureID>(
                    static_cast<uintptr_t>(
                        m_SceneFramebuffer.getColorTexture())),
                viewportSize,
                ImVec2(0, 1),
                ImVec2(1, 0));

            m_ViewportHovered = ImGui::IsItemHovered();

            ImGui::SetCursorPos(ImVec2(18.0f, 58.0f));
            ImGui::BeginChild("ViewportOverlay", ImVec2(190.0f, 86.0f), true);
            ImGui::TextDisabled("CAMERA");
            ImGui::Text("RMB orbit | MMB pan | Wheel zoom");
            ImGui::Text("Z/X move along world Z");
            ImGui::Text("Grid  %s", m_ShowGrid ? "ON" : "OFF");
            ImGui::Text("Origin  (0, 0, 0)");
            ImGui::EndChild();
        }

        ImGui::End();
    }

    void Editor::drawInspector()
    {
        ImGui::Begin("Inspector");

        ImGui::TextUnformatted("robot_alpha");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 38.0f);
        ImGui::TextDisabled("LINK");
        ImGui::Separator();

        ImGui::TextDisabled("General");
        char entityName[] = "robot_alpha";
        ImGui::InputText("Name", entityName, sizeof(entityName));

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

    void Editor::drawConsole()
    {
        ImGui::Begin("Console");

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

    void Editor::endFrame()
    {
        if (!m_Initialized)
            return;

        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData());
    }

    void Editor::shutdown()
    {
        if (!m_Initialized)
            return;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImGui::DestroyContext();

        m_Initialized = false;
    }

}