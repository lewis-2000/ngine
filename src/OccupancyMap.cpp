#include "OccupancyMap.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>

#include <glm/geometric.hpp>

namespace Mara
{
    namespace
    {
        constexpr float kMillimetresToMetres = 0.001f;
    }

    OccupancyMap::OccupancyMap(float voxelSize)
        : m_VoxelSize(std::max(0.01f, voxelSize))
    {
    }

    void OccupancyMap::clear()
    {
        m_Voxels.clear();
        m_Keys.clear();
        m_KeySet.clear();
        m_Indices.clear();
    }

    void OccupancyMap::addBoxObstacle(
        const glm::vec3 &minimum,
        const glm::vec3 &maximum)
    {
        const glm::ivec3 first = coordinateFor(minimum);
        const glm::ivec3 last = coordinateFor(maximum);
        for (int x = first.x; x <= last.x; ++x)
            for (int y = first.y; y <= last.y; ++y)
                for (int z = first.z; z <= last.z; ++z)
                {
                    const std::int64_t voxelKey = key({x, y, z});
                    if (!m_KeySet.insert(voxelKey).second)
                        continue;
                    m_Keys.push_back(voxelKey);
                    m_Indices.emplace(voxelKey, m_Voxels.size());
                    m_Voxels.push_back({{x, y, z}, 255});
                }
    }

    std::int64_t OccupancyMap::key(const glm::ivec3 &coordinate) const
    {
        constexpr std::int64_t mask = (1LL << 21) - 1;
        const auto pack = [mask](int value) -> std::int64_t
        {
            return static_cast<std::int64_t>(value) & mask;
        };
        return (pack(coordinate.x) << 42) |
               (pack(coordinate.y) << 21) |
               pack(coordinate.z);
    }

    glm::ivec3 OccupancyMap::coordinateFor(const glm::vec3 &position) const
    {
        return glm::ivec3(
            static_cast<int>(std::floor(position.x / m_VoxelSize)),
            static_cast<int>(std::floor(position.y / m_VoxelSize)),
            static_cast<int>(std::floor(position.z / m_VoxelSize)));
    }

    void OccupancyMap::integrateDepth(
        const std::vector<std::uint16_t> &depth,
        int width,
        int height,
        float depthScale,
        float focalLengthX,
        float focalLengthY,
        float principalX,
        float principalY)
    {
        if (width <= 0 || height <= 0 ||
            depth.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
            focalLengthX <= 0.0f || focalLengthY <= 0.0f)
            return;

        std::unordered_map<std::int64_t, std::uint8_t> updates;
        updates.reserve(depth.size() / 4u);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::uint16_t raw = depth[static_cast<std::size_t>(y) * width + x];
                if (raw == 0)
                    continue;

                const float z = static_cast<float>(raw) * depthScale * kMillimetresToMetres;
                const glm::vec3 point(
                    (static_cast<float>(x) - principalX) * z / focalLengthX,
                    -(static_cast<float>(y) - principalY) * z / focalLengthY,
                    z);
                const glm::ivec3 coordinate = coordinateFor(point);
                const std::int64_t voxelKey = key(coordinate);
                auto [it, inserted] = updates.emplace(voxelKey, 1);
                if (!inserted)
                    it->second = static_cast<std::uint8_t>(
                        std::min(255, static_cast<int>(it->second) + 1));
            }
        }

        for (const auto &[voxelKey, weight] : updates)
        {
            const auto existing = m_Indices.find(voxelKey);
            if (existing != m_Indices.end())
            {
                const std::size_t index = existing->second;
                m_Voxels[index].weight =
                    static_cast<std::uint8_t>(
                        std::min(255, static_cast<int>(m_Voxels[index].weight) +
                            static_cast<int>(weight)));
                continue;
            }

            const std::int64_t packed = voxelKey;
            constexpr std::int64_t mask = (1LL << 21) - 1;
            const auto unpack = [mask](std::int64_t value) -> int
            {
                value &= mask;
                if ((value & (1LL << 20)) != 0)
                    value |= ~mask;
                return static_cast<int>(value);
            };
            m_Keys.push_back(packed);
            m_KeySet.insert(packed);
            m_Indices.emplace(packed, m_Voxels.size());
            m_Voxels.push_back({{
                unpack(packed >> 42),
                unpack(packed >> 21),
                unpack(packed)}, weight});
        }
    }

    bool OccupancyMap::isOccupied(const glm::vec3 &position) const
    {
        const std::int64_t voxelKey = key(coordinateFor(position));
        return m_KeySet.contains(voxelKey);
    }

    bool OccupancyMap::intersectsBox(
        const glm::vec3 &minimum,
        const glm::vec3 &maximum) const
    {
        const glm::ivec3 first = coordinateFor(minimum);
        const glm::ivec3 last = coordinateFor(maximum);
        for (int x = first.x; x <= last.x; ++x)
            for (int y = first.y; y <= last.y; ++y)
                for (int z = first.z; z <= last.z; ++z)
                    if (isOccupied(glm::vec3(x, y, z) * m_VoxelSize))
                        return true;
        return false;
    }

    bool OccupancyMap::raycast(
        const glm::vec3 &origin,
        const glm::vec3 &direction,
        float maxDistance,
        OccupancyRayHit &hit) const
    {
        const float length = glm::length(direction);
        if (length <= 0.0001f || maxDistance <= 0.0f)
            return false;

        const glm::vec3 step = direction / length;
        constexpr float stepSize = 0.01f;
        for (float distance = 0.0f; distance <= maxDistance; distance += stepSize)
        {
            const glm::vec3 position = origin + step * distance;
            if (isOccupied(position))
            {
                hit.position = position;
                hit.distance = distance;
                return true;
            }
        }
        return false;
    }

    void OccupancyMap::setVoxelSize(float size)
    {
        const float clamped = std::max(0.01f, size);
        if (std::abs(clamped - m_VoxelSize) < 0.0001f)
            return;
        m_VoxelSize = clamped;
        clear();
    }

    bool OccupancyMap::save(const std::filesystem::path &path) const
    {
        std::ofstream output(path);
        if (!output)
            return false;
        output << "NGINE_OCCUPANCY_MAP 1 " << m_VoxelSize << '\n';
        for (const OccupiedVoxel &voxel : m_Voxels)
            output << voxel.coordinate.x << ' '
                   << voxel.coordinate.y << ' '
                   << voxel.coordinate.z << ' '
                   << static_cast<int>(voxel.weight) << '\n';
        return static_cast<bool>(output);
    }

    bool OccupancyMap::load(const std::filesystem::path &path)
    {
        std::ifstream input(path);
        if (!input)
            return false;

        std::string header;
        int version = 0;
        float voxelSize = 0.0f;
        if (!(input >> header >> version >> voxelSize) ||
            header != "NGINE_OCCUPANCY_MAP" || version != 1 || voxelSize <= 0.0f)
            return false;

        std::vector<OccupiedVoxel> loaded;
        std::vector<std::int64_t> loadedKeys;
        std::unordered_set<std::int64_t> loadedKeySet;
        std::unordered_map<std::int64_t, std::size_t> loadedIndices;
        OccupiedVoxel voxel;
        int weight = 0;
        while (input >> voxel.coordinate.x >> voxel.coordinate.y >>
               voxel.coordinate.z >> weight)
        {
            voxel.weight = static_cast<std::uint8_t>(
                std::clamp(weight, 1, 255));
            loadedKeys.push_back(key(voxel.coordinate));
            loadedKeySet.insert(loadedKeys.back());
            loadedIndices.emplace(loadedKeys.back(), loaded.size());
            loaded.push_back(voxel);
        }
        if (!input.eof())
            return false;

        m_VoxelSize = voxelSize;
        m_Voxels = std::move(loaded);
        m_Keys = std::move(loadedKeys);
        m_KeySet = std::move(loadedKeySet);
        m_Indices = std::move(loadedIndices);
        return true;
    }
}
