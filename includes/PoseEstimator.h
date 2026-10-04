#pragma once

#include "PoseFrame.h"

#include <memory>
#include <string>

namespace Mara
{
    class PoseEstimator
    {
    public:
        virtual ~PoseEstimator() = default;

        virtual void update(
            float deltaTime,
            const unsigned char *rgbaPixels,
            int width,
            int height) = 0;
        virtual const PoseFrame &frame() const = 0;
        virtual const char *backendName() const = 0;
    };

    class MockPoseEstimator final : public PoseEstimator
    {
    public:
        void update(
            float deltaTime,
            const unsigned char *rgbaPixels,
            int width,
            int height) override;
        const PoseFrame &frame() const override { return m_Frame; }
        const char *backendName() const override { return "Mock pose estimator"; }

    private:
        float m_Time = 0.0f;
        PoseFrame m_Frame;
    };

    class MoveNetPoseEstimator final : public PoseEstimator
    {
    public:
        explicit MoveNetPoseEstimator(const std::string &modelPath);
        ~MoveNetPoseEstimator() override;

        void update(
            float deltaTime,
            const unsigned char *rgbaPixels,
            int width,
            int height) override;
        const PoseFrame &frame() const override { return m_Frame; }
        const char *backendName() const override;
        bool isAvailable() const;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_Implementation;
        PoseFrame m_Frame;
        std::string m_Error;
        float m_Time = 0.0f;
    };

    std::unique_ptr<PoseEstimator> createPoseEstimator();
}
