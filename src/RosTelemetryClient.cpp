#include "RosTelemetryClient.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <regex>
#include <sstream>

#include <stb_image.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Mara
{
    namespace
    {
        std::uint64_t nowMs()
        {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }

        void closeSocket(std::intptr_t &socket)
        {
            if (socket < 0)
                return;
#ifdef _WIN32
            closesocket(static_cast<SOCKET>(socket));
#else
            close(static_cast<int>(socket));
#endif
            socket = -1;
        }

        bool extractNumber(
            const std::string &text,
            const std::string &key,
            float &value)
        {
            const std::regex expression(
                "\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
            std::smatch match;
            if (!std::regex_search(text, match, expression))
                return false;
            value = std::strtof(match[1].str().c_str(), nullptr);
            return true;
        }

        bool extractInteger(
            const std::string &text,
            const std::string &key,
            int &value)
        {
            float number = 0.0f;
            if (!extractNumber(text, key, number))
                return false;
            value = static_cast<int>(number);
            return true;
        }

        std::vector<std::uint8_t> decodeBase64(const std::string &encoded)
        {
            static constexpr char alphabet[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::vector<std::uint8_t> decoded;
            int value = 0;
            int bits = -8;
            for (const unsigned char character : encoded)
            {
                if (character == '=')
                    break;
                const char *position = std::strchr(alphabet, character);
                if (!position)
                    continue;
                value = (value << 6) + static_cast<int>(position - alphabet);
                bits += 6;
                if (bits >= 0)
                {
                    decoded.push_back(static_cast<std::uint8_t>((value >> bits) & 0xff));
                    bits -= 8;
                }
            }
            return decoded;
        }

        bool extractString(
            const std::string &text,
            const std::string &key,
            std::string &value)
        {
            const std::string marker = "\"" + key + "\"";
            const std::size_t keyPosition = text.find(marker);
            if (keyPosition == std::string::npos)
                return false;

            const std::size_t separator = text.find(':', keyPosition + marker.size());
            if (separator == std::string::npos)
                return false;

            const std::size_t firstQuote = text.find('"', separator + 1);
            if (firstQuote == std::string::npos)
                return false;

            const std::size_t secondQuote = text.find('"', firstQuote + 1);
            if (secondQuote == std::string::npos)
                return false;

            value = text.substr(firstQuote + 1, secondQuote - firstQuote - 1);
            return true;
        }
    }

    RosTelemetryClient::~RosTelemetryClient()
    {
        shutdown();
    }

    bool RosTelemetryClient::connect(const std::string &host, std::uint16_t port)
    {
        shutdown();
        m_Host = host;
        m_Port = port;

#ifdef _WIN32
        static bool winsockInitialized = false;
        if (!winsockInitialized)
        {
            WSADATA data{};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            {
                setError("Winsock initialization failed");
                return false;
            }
            winsockInitialized = true;
        }
#endif

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        addrinfo *addresses = nullptr;
        const std::string portString = std::to_string(port);
        if (getaddrinfo(host.c_str(), portString.c_str(), &hints, &addresses) != 0)
        {
            setError("Could not resolve telemetry host");
            return false;
        }

        for (addrinfo *address = addresses; address; address = address->ai_next)
        {
            const auto socketHandle = ::socket(
                address->ai_family,
                address->ai_socktype,
                address->ai_protocol);
            if (socketHandle < 0)
                continue;

            if (::connect(socketHandle, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
            {
                m_Socket = static_cast<std::intptr_t>(socketHandle);
                break;
            }
#ifdef _WIN32
            closesocket(static_cast<SOCKET>(socketHandle));
#else
            close(socketHandle);
#endif
        }
        freeaddrinfo(addresses);

        if (m_Socket < 0)
        {
            setError("Telemetry relay is unavailable");
            return false;
        }

#ifdef _WIN32
        u_long nonBlocking = 1;
        ioctlsocket(static_cast<SOCKET>(m_Socket), FIONBIO, &nonBlocking);
#else
        const int flags = fcntl(static_cast<int>(m_Socket), F_GETFL, 0);
        fcntl(static_cast<int>(m_Socket), F_SETFL, flags | O_NONBLOCK);
#endif
        m_Connected = true;
        m_LastError.clear();
        return true;
    }

    void RosTelemetryClient::shutdown()
    {
        closeSocket(m_Socket);
        m_Connected = false;
        m_ReceiveBuffer.clear();
    }

    void RosTelemetryClient::setError(const std::string &error)
    {
        m_LastError = error;
        shutdown();
    }

    bool RosTelemetryClient::stale() const
    {
        return !m_HasData || nowMs() - m_LastDataTimeMs > 1000;
    }

    void RosTelemetryClient::update()
    {
        if (!m_Connected)
            return;

        char buffer[8192];
        while (true)
        {
#ifdef _WIN32
            const int count = recv(
                static_cast<SOCKET>(m_Socket),
                buffer,
                sizeof(buffer),
                0);
#else
            const int count = recv(static_cast<int>(m_Socket), buffer, sizeof(buffer), 0);
#endif
            if (count > 0)
            {
                m_ReceiveBuffer.append(buffer, static_cast<std::size_t>(count));
                if (m_ReceiveBuffer.size() > 8u * 1024u * 1024u)
                {
                    setError("Telemetry frame is too large");
                    return;
                }
                continue;
            }
            if (count == 0)
            {
                setError("Telemetry relay disconnected");
                return;
            }
#ifdef _WIN32
            const int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
#else
            if (errno != EWOULDBLOCK && errno != EAGAIN)
#endif
                setError("Telemetry receive failed");
            break;
        }

        std::size_t newline = 0;
        while ((newline = m_ReceiveBuffer.find('\n')) != std::string::npos)
        {
            std::string line = m_ReceiveBuffer.substr(0, newline);
            m_ReceiveBuffer.erase(0, newline + 1);
            if (!line.empty())
                parseLine(line);
        }
    }

    bool RosTelemetryClient::parseLine(const std::string &line)
    {
        int integer = 0;
        float value = 0.0f;
        if (extractInteger(line, "bodycontrol_state", integer))
            m_Snapshot.bodyControlState = integer;
        if (extractInteger(line, "process_state", integer))
            m_Snapshot.processState = integer;
        if (extractNumber(line, "battery_voltage", value))
            m_Snapshot.batteryVoltage = value;
        if (extractNumber(line, "battery_current", value))
            m_Snapshot.batteryCurrent = value;
        if (extractNumber(line, "battery_power", value))
            m_Snapshot.batteryPower = value;
        if (extractNumber(line, "imu_roll", value))
            m_Snapshot.imuRoll = value;
        if (extractNumber(line, "imu_pitch", value))
            m_Snapshot.imuPitch = value;
        if (extractNumber(line, "imu_yaw", value))
            m_Snapshot.imuYaw = value;

        std::string encodedCamera;
        if (extractString(line, "camera_jpeg", encodedCamera) &&
            encodedCamera.size() <= 4u * 1024u * 1024u)
        {
            const std::vector<std::uint8_t> jpeg = decodeBase64(encodedCamera);
            int width = 0;
            int height = 0;
            int channels = 0;
            unsigned char *rgb = nullptr;
            if (!jpeg.empty())
            {
                rgb = stbi_load_from_memory(
                    jpeg.data(),
                    static_cast<int>(jpeg.size()),
                    &width,
                    &height,
                    &channels,
                    3);
            }
            if (rgb)
            {
                m_Snapshot.cameraRgb.assign(
                    rgb,
                    rgb + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u);
                m_Snapshot.cameraWidth = width;
                m_Snapshot.cameraHeight = height;
                ++m_Snapshot.cameraSequence;
                stbi_image_free(rgb);
            }
        }

        const std::size_t cameraPosition = line.find("\"camera_jpeg\"");
        const std::string motorData =
            cameraPosition == std::string::npos
                ? line
                : line.substr(0, cameraPosition);
        const std::regex motorExpression(
            "\\{\\s*\"id\"\\s*:\\s*([0-9]+).*?\"position\"\\s*:\\s*(-?[0-9.eE+]+).*?\"speed\"\\s*:\\s*(-?[0-9.eE+]+).*?\"current\"\\s*:\\s*(-?[0-9.eE+]+).*?\"temperature\"\\s*:\\s*(-?[0-9.eE+]+).*?\"error\"\\s*:\\s*([0-9]+)\\s*\\}");
        for (std::sregex_iterator it(motorData.begin(), motorData.end(), motorExpression), end;
             it != end;
             ++it)
        {
            MotorTelemetry motor;
            motor.id = std::stoi((*it)[1].str());
            motor.position = std::strtof((*it)[2].str().c_str(), nullptr);
            motor.speed = std::strtof((*it)[3].str().c_str(), nullptr);
            motor.current = std::strtof((*it)[4].str().c_str(), nullptr);
            motor.temperature = std::strtof((*it)[5].str().c_str(), nullptr);
            motor.error = static_cast<std::uint32_t>(std::stoul((*it)[6].str()));

            const auto existing = std::find_if(
                m_Snapshot.motors.begin(),
                m_Snapshot.motors.end(),
                [&motor](const MotorTelemetry &item) { return item.id == motor.id; });
            if (existing == m_Snapshot.motors.end())
                m_Snapshot.motors.push_back(motor);
            else
                *existing = motor;
        }

        if (line.find("\"type\":\"telemetry\"") == std::string::npos &&
            line.find("\"type\": \"telemetry\"") == std::string::npos)
            return false;

        m_HasData = true;
        m_LastDataTimeMs = nowMs();
        ++m_Snapshot.sequence;
        return true;
    }
}
