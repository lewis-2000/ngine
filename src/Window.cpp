#include "Window.h"
#include "Logger.h"

namespace Mara
{
    Window::Window(int width, int height, const char *title)
        : m_Width(width), m_Height(height)
    {
        // Initialize GLFW
        if (!glfwInit())
        {
            Logger::error("Failed to initialize GLFW");
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Set OpenGL context hints
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        // Keep render targets in physical pixels on HiDPI displays.
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
        // Keep requested window size stable across monitor scale factors.
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
        // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS
#endif

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        // 3. Create the GLFW window
        m_Window = glfwCreateWindow(m_Width, m_Height, title, nullptr, nullptr);
        if (!m_Window)
        {
            const char *description;
            int code = glfwGetError(&description);
            Logger::error(std::string("Failed to create GLFW window: ") + (description ? description : "unknown"));
            glfwTerminate();
            Logger::runtimeError("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetFramebufferSizeCallback(m_Window, framebufferSizeCallback);

        // Make context current
        glfwMakeContextCurrent(m_Window);

        glfwGetFramebufferSize(m_Window, &m_Width, &m_Height);

        // Initialize GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            Logger::error("Failed to initialize GLAD");
            glfwDestroyWindow(m_Window);
            glfwTerminate();
            throw std::runtime_error("Failed to initialize GLAD");
        }

        Logger::information("GLFW window created successfully");
    }

    Window::~Window()
    {
        if (m_Window)
        {
            glfwDestroyWindow(m_Window);
            glfwTerminate();
        }
    }

    void Window::initialize()
    {
        // Additional initialization code can go here
    }

    void Window::pollEvents()
    {
        glfwPollEvents();
    }

    void Window::show()
    {
        if (!m_Window)
            return;

        glfwShowWindow(m_Window);
        glfwFocusWindow(m_Window);
    }

    void Window::framebufferSizeCallback(GLFWwindow *window, int width, int height)
    {
        auto *instance = static_cast<Window *>(glfwGetWindowUserPointer(window));
        if (!instance)
            return;

        instance->m_Width = width;
        instance->m_Height = height;
        glViewport(0, 0, width, height);
    }

}