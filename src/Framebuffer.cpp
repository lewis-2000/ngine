#include "Framebuffer.h"

namespace Mara
{

    Framebuffer::~Framebuffer()
    {
        destroy();
    }

    void Framebuffer::initialize(int width, int height)
    {
        m_Width = width;
        m_Height = height;

        create();
    }

    void Framebuffer::create()
    {
        // ------------------------------------------------------------
        // Create framebuffer object
        // ------------------------------------------------------------

        glGenFramebuffers(1, &m_Framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_Framebuffer);

        // ------------------------------------------------------------
        // Create color texture
        // ------------------------------------------------------------

        glGenTextures(1, &m_ColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_ColorTexture);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB,
            m_Width,
            m_Height,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            nullptr);

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            GL_LINEAR);

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            GL_LINEAR);

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            GL_CLAMP_TO_EDGE);

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            GL_CLAMP_TO_EDGE);

        // Attach color texture to framebuffer
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            m_ColorTexture,
            0);

        // ------------------------------------------------------------
        // Create depth + stencil renderbuffer
        // ------------------------------------------------------------

        glGenRenderbuffers(1, &m_DepthStencil);

        glBindRenderbuffer(
            GL_RENDERBUFFER,
            m_DepthStencil);

        glRenderbufferStorage(
            GL_RENDERBUFFER,
            GL_DEPTH24_STENCIL8,
            m_Width,
            m_Height);

        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER,
            m_DepthStencil);

        // ------------------------------------------------------------
        // Check framebuffer
        // ------------------------------------------------------------

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE)
        {
            // TODO: replace with your engine logger
        }

        // Unbind
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::bind()
    {
        glBindFramebuffer(
            GL_FRAMEBUFFER,
            m_Framebuffer);

        glViewport(
            0,
            0,
            m_Width,
            m_Height);
    }

    void Framebuffer::unbind()
    {
        glBindFramebuffer(
            GL_FRAMEBUFFER,
            0);
    }

    GLuint Framebuffer::getColorTexture() const
    {
        return m_ColorTexture;
    }

    void Framebuffer::resize(int width, int height)
    {
        if (width == m_Width && height == m_Height)
            return;

        if (width <= 0 || height <= 0)
            return;

        destroy();

        m_Width = width;
        m_Height = height;

        create();
    }

    void Framebuffer::destroy()
    {
        if (m_DepthStencil)
        {
            glDeleteRenderbuffers(
                1,
                &m_DepthStencil);

            m_DepthStencil = 0;
        }

        if (m_ColorTexture)
        {
            glDeleteTextures(
                1,
                &m_ColorTexture);

            m_ColorTexture = 0;
        }

        if (m_Framebuffer)
        {
            glDeleteFramebuffers(
                1,
                &m_Framebuffer);

            m_Framebuffer = 0;
        }
    }

}