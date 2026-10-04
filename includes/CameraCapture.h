#pragma once

#include <string>
#include <vector>

namespace Mara
{
    class CameraCapture
    {
    public:
        ~CameraCapture();

        bool open(int deviceIndex = 0, int width = 640, int height = 480);
        void close();
        bool update();
        bool isOpen() const { return m_Open; }
        unsigned int texture() const { return m_Texture; }
        int width() const { return m_Width; }
        int height() const { return m_Height; }
        const std::vector<unsigned char> &pixels() const { return m_Pixels; }
        const std::string &lastError() const { return m_LastError; }

    private:
        bool m_Open = false;
        unsigned int m_Texture = 0;
        int m_Width = 0;
        int m_Height = 0;
        std::string m_LastError;
        std::vector<unsigned char> m_Pixels;
        struct Implementation;
        Implementation *m_Implementation = nullptr;
    };
}
