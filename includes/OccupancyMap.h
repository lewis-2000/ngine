#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/vec3.hpp>

namespace Mara
{
    struct OccupiedVoxel
    {
        glm::ivec3 coordinate{0};
        std::uint8_t weight = 0;
    };

    struct OccupancyRayHit
    {
        glm::vec3 position{0.0f};
        float distance = 0.0f;
    };

    class OccupancyMap
    {
    public:
        explicit OccupancyMap(float voxelSize = 0.05f);

        void clear();
        void addBoxObstacle(const glm::vec3 &minimum, const glm::vec3 &maximum);
        void integrateDepth(
            const std::vector<std::uint16_t> &depth,
            int width,
            int height,
            float depthScale,
            float focalLengthX,
            float focalLengthY,
            float principalX,
            float principalY);

        bool isOccupied(const glm::vec3 &position) const;
        bool intersectsBox(const glm::vec3 &minimum, const glm::vec3 &maximum) const;
        bool raycast(
            const glm::vec3 &origin,
            const glm::vec3 &direction,
            float maxDistance,
            OccupancyRayHit &hit) const;
        std::size_t occupiedCount() const { return m_Voxels.size(); }
        float voxelSize() const { return m_VoxelSize; }
        void setVoxelSize(float size);
        const std::vector<OccupiedVoxel> &voxels() const { return m_Voxels; }

        bool save(const std::filesystem::path &path) const;
        bool load(const std::filesystem::path &path);

    private:
        std::int64_t key(const glm::ivec3 &coordinate) const;
        glm::ivec3 coordinateFor(const glm::vec3 &position) const;

        float m_VoxelSize;
        std::vector<OccupiedVoxel> m_Voxels;
        std::vector<std::int64_t> m_Keys;
        std::unordered_set<std::int64_t> m_KeySet;
        std::unordered_map<std::int64_t, std::size_t> m_Indices;
    };
}
