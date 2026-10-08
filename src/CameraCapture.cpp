#include "CameraCapture.h"

#include <glad/glad.h>

#ifdef NGINE_ENABLE_OPENCV
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace Mara
{
    struct CameraCapture::Implementation
    {
#ifdef NGINE_ENABLE_OPENCV
        cv::VideoCapture capture;
        cv::Mat frame;
        cv::Mat rgba;
#endif
    };

    CameraCapture::~CameraCapture()
    {
        close();
    }

    bool CameraCapture::open(int deviceIndex, int width, int height)
    {
        close();
        m_LastError.clear();
        m_Implementation = new Implementation();

#ifdef NGINE_ENABLE_OPENCV
        if (!m_Implementation->capture.open(deviceIndex))
        {
            m_LastError = "Unable to open webcam device " + std::to_string(deviceIndex);
            return false;
        }

        m_Implementation->capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
        m_Implementation->capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        m_Open = true;
        return update();
#else
        (void)deviceIndex;
        (void)width;
        (void)height;
        m_LastError = "OpenCV support is disabled. Reconfigure with -DNGINE_ENABLE_OPENCV=ON.";
        return false;
#endif
    }

    void CameraCapture::close()
    {
#ifdef NGINE_ENABLE_OPENCV
        if (m_Implementation)
            m_Implementation->capture.release();
#endif

        if (m_Texture != 0)
        {
            glDeleteTextures(1, &m_Texture);
            m_Texture = 0;
        }

        m_Open = false;
        m_Width = 0;
        m_Height = 0;
        m_Pixels.clear();
        delete m_Implementation;
        m_Implementation = nullptr;
    }

    bool CameraCapture::update()
    {
#ifdef NGINE_ENABLE_OPENCV
        if (!m_Open || !m_Implementation->capture.read(m_Implementation->frame))
        {
            m_LastError = "Unable to read a frame from the webcam.";
            return false;
        }

        cv::cvtColor(
            m_Implementation->frame,
            m_Implementation->rgba,
            cv::COLOR_BGR2RGBA);

        if (m_Texture == 0)
        {
            glGenTextures(1, &m_Texture);
            glBindTexture(GL_TEXTURE_2D, m_Texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        m_Width = m_Implementation->rgba.cols;
        m_Height = m_Implementation->rgba.rows;
        m_Pixels.assign(
            m_Implementation->rgba.data,
            m_Implementation->rgba.data +
                (m_Width * m_Height * 4));
        glBindTexture(GL_TEXTURE_2D, m_Texture);
        GLint unpackAlignment = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            m_Width,
            m_Height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            m_Implementation->rgba.data);
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
#else
        return false;
#endif
    }
}
