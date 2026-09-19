#pragma once

#include <glad/glad.h>

namespace Mara
{

    class Framebuffer
    {
    public:
        Framebuffer() = default;
        ~Framebuffer();

        void initialize(int width, int height);
        void resize(int width, int height);

        void bind();
        void unbind();

        GLuint getColorTexture() const;
        int getWidth() const { return m_Width; }
        int getHeight() const { return m_Height; }

    private:
        void create();
        void destroy();

    private:
        GLuint m_Framebuffer = 0;
        GLuint m_ColorTexture = 0;
        GLuint m_DepthStencil = 0;

        int m_Width = 0;
        int m_Height = 0;
    };

}