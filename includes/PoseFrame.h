#pragma once

#include <array>

#include <glm/vec2.hpp>

namespace Mara
{
    struct PoseLandmark
    {
        glm::vec2 position{0.0f};
        float confidence = 0.0f;
    };

    struct PoseFrame
    {
        static constexpr std::size_t LandmarkCount = 17;

        std::array<PoseLandmark, LandmarkCount> landmarks{};
        bool valid = false;
        float timestamp = 0.0f;
    };
}
