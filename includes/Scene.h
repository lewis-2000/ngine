#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/vec3.hpp>

namespace Mara
{
    using Entity = std::uint32_t;
    constexpr Entity NullEntity = 0;

    struct TransformComponent
    {
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};
    };

    class Scene
    {
    public:
        Entity createEntity(const std::string &name);
        void destroyEntity(Entity entity);

        bool isAlive(Entity entity) const;
        const std::vector<Entity> &entities() const { return m_Entities; }

        const std::string *name(Entity entity) const;
        TransformComponent *transform(Entity entity);
        const TransformComponent *transform(Entity entity) const;

    private:
        Entity m_NextEntity = 1;
        std::vector<Entity> m_Entities;
        std::unordered_map<Entity, std::string> m_Names;
        std::unordered_map<Entity, TransformComponent> m_Transforms;
    };
}
