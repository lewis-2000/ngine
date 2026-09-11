#pragma once

#include <memory>

#include "Window.h"

class EBO;
class Shader;
class VAO;
class VBO;

namespace Mara
{
    class App
    {
    public:
        App();
        ~App();

        App(const App &) = delete;
        App &operator=(const App &) = delete;

        void initialize();
        void run();

    private:
        void renderFrame();
        void shutdown();

        std::unique_ptr<Window> m_Window;
        std::unique_ptr<VAO> m_VAO;
        std::unique_ptr<VBO> m_VBO;
        std::unique_ptr<EBO> m_EBO;
        std::unique_ptr<Shader> m_Shader;
    };
}
