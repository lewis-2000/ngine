#pragma once

#include <memory>

#include "Camera.h"
#include "Editor.h"
#include "Plane.h"
#include "Robot.h"
#include "Scene.h"
#include "Shader.h"
#include "Window.h"

namespace Mara
{
    class Renderer
    {
    public:
        Renderer(Window &window, Editor &editor, Robot &robot, Plane &plane);

        void initialize();
        void render(const Scene &scene, Entity robotEntity, Entity groundEntity, const Camera &camera);

    private:
        Window &m_Window;
        Editor &m_Editor;
        Robot &m_Robot;
        Plane &m_Plane;
        std::unique_ptr<Shader> m_Shader;
    };
}
