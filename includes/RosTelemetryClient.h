#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Mara
{
    struct MotorTelemetry
    {
        int id = 0;
        float position = 0.0f;
        float speed = 0.0f;
        float current = 0.0f;
        float temperature = 0.0f;
        std::uint32_t error = 0;
    };

    struct RosTelemetrySnapshot
    {
        std::vector<MotorTelemetry> motors;
        int bodyControlState = -1;
        int processState = -1;
        float batteryVoltage = 0.0f;
        float batteryCurrent = 0.0f;
        float batteryPower = 0.0f;
        float imuRoll = 0.0f;
        float imuPitch = 0.0f;
        float imuYaw = 0.0f;
        std::uint64_t sequence = 0;
    };

    class RosTelemetryClient
    {
    public:
        RosTelemetryClient() = default;
        ~RosTelemetryClient();

        RosTelemetryClient(const RosTelemetryClient &) = delete;
        RosTelemetryClient &operator=(const RosTelemetryClient &) = delete;

        void update();
        void shutdown();

        bool connect(const std::string &host, std::uint16_t port);
        bool connected() const { return m_Connected; }
        bool hasData() const { return m_HasData; }
        bool stale() const;

        const RosTelemetrySnapshot &snapshot() const { return m_Snapshot; }
        const std::string &lastError() const { return m_LastError; }
        const std::string &host() const { return m_Host; }
        std::uint16_t port() const { return m_Port; }

    private:
        void setError(const std::string &error);
        bool parseLine(const std::string &line);

        std::intptr_t m_Socket = -1;
        bool m_Connected = false;
        bool m_HasData = false;
        std::string m_Host = "192.168.41.1";
        std::uint16_t m_Port = 8765;
        std::string m_ReceiveBuffer;
        std::string m_LastError;
        RosTelemetrySnapshot m_Snapshot;
        std::uint64_t m_LastDataTimeMs = 0;
    };
}
