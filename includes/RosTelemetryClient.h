#pragma once

#include <cstdint>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
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
        std::vector<std::uint8_t> cameraRgb;
        int cameraWidth = 0;
        int cameraHeight = 0;
        std::uint64_t cameraSequence = 0;
        std::vector<std::uint16_t> depth;
        int depthWidth = 0;
        int depthHeight = 0;
        std::uint64_t depthSequence = 0;
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
        bool connected() const;
        bool hasData() const;
        bool stale() const;

        const RosTelemetrySnapshot &snapshot() const { return m_Snapshot; }
        std::string lastError() const;
        const std::string &host() const { return m_Host; }
        std::uint16_t port() const { return m_Port; }

    private:
        void setError(const std::string &error);
        bool parseLine(const std::string &line, RosTelemetrySnapshot &snapshot);
        void receiveLoop();

        std::intptr_t m_Socket = -1;
        bool m_Connected = false;
        bool m_HasData = false;
        std::string m_Host = "192.168.41.1";
        std::uint16_t m_Port = 8765;
        std::string m_ReceiveBuffer;
        std::string m_LastError;
        RosTelemetrySnapshot m_Snapshot;
        RosTelemetrySnapshot m_WorkerSnapshot;
        RosTelemetrySnapshot m_PendingSnapshot;
        bool m_PendingData = false;
        mutable std::mutex m_StateMutex;
        std::atomic<bool> m_StopRequested = false;
        std::thread m_ReceiveThread;
        std::uint64_t m_LastDataTimeMs = 0;
    };
}
