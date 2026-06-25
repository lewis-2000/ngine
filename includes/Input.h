#pragma once
#include <GLFW/glfw3.h>

namespace MaraGl
{
    class Input
    {
    public:
        static void Init(GLFWwindow *window);

        static bool IsKeyPressed(int key);
        static bool IsMouseButtonPressed(int button);

        static float GetMouseX();
        static float GetMouseY();
        static float GetMouseDeltaX();
        static float GetMouseDeltaY();

        static void Update(); // Call once per frame

    private:
        static GLFWwindow *s_Window;

        static double s_LastMouseX;
        static double s_LastMouseY;
        static float s_DeltaX;
        static float s_DeltaY;
    };
} // namespace MaraGl