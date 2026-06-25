#pragma once

#include <glad/glad.h>

#include <GLFW/glfw3.h>

namespace MaraGl
{
    class Window
    {
    public:
        // Constructor / destructor
        Window(int width, int height, const char *title);
        ~Window();

        // Lifecycle
        void initialize();
        void pollEvents();
        void show();

        // Getters
        int getWidth() const { return m_Width; }
        int getHeight() const { return m_Height; }
        GLFWwindow *getWindow() const { return m_Window; }

    private:
        GLFWwindow *m_Window = nullptr;
        int m_Width = 0;
        int m_Height = 0;
    };
}