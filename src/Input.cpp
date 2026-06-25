#include "Input.h"

namespace MaraGl
{
    GLFWwindow *Input::s_Window = nullptr;
    double Input::s_LastMouseX = 0.0;
    double Input::s_LastMouseY = 0.0;
    float Input::s_DeltaX = 0.0f;
    float Input::s_DeltaY = 0.0f;

    void Input::Init(GLFWwindow *window)
    {
        s_Window = window;
    }

    bool Input::IsKeyPressed(int key)
    {
        return glfwGetKey(s_Window, key) == GLFW_PRESS;
    }

    bool Input::IsMouseButtonPressed(int button)
    {
        return glfwGetMouseButton(s_Window, button) == GLFW_PRESS;
    }

    void Input::Update()
    {
        double x, y;
        glfwGetCursorPos(s_Window, &x, &y);

        s_DeltaX = (float)(x - s_LastMouseX);
        s_DeltaY = (float)(y - s_LastMouseY);

        s_LastMouseX = x;
        s_LastMouseY = y;
    }

    float Input::GetMouseX() { return (float)s_LastMouseX; }
    float Input::GetMouseY() { return (float)s_LastMouseY; }
    float Input::GetMouseDeltaX() { return s_DeltaX; }
    float Input::GetMouseDeltaY() { return s_DeltaY; }
} // namespace MaraGl