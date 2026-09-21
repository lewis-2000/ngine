#include "Scene.h"

#include <algorithm>

namespace Mara
{
    Entity Scene::createEntity(const std::string &name)
    {
        const Entity entity = m_NextEntity++;
        m_Entities.push_back(entity);
        m_Names.emplace(entity, name);
        m_Transforms.emplace(entity, TransformComponent{});
        return entity;
    }

    void Scene::destroyEntity(Entity entity)
    {
        if (!isAlive(entity))
            return;

        m_Entities.erase(
            std::remove(m_Entities.begin(), m_Entities.end(), entity),
            m_Entities.end());
        m_Names.erase(entity);
        m_Transforms.erase(entity);
    }

    bool Scene::isAlive(Entity entity) const
    {
        return entity != NullEntity && m_Transforms.find(entity) != m_Transforms.end();
    }

    const std::string *Scene::name(Entity entity) const
    {
        const auto it = m_Names.find(entity);
        return it != m_Names.end() ? &it->second : nullptr;
    }

    TransformComponent *Scene::transform(Entity entity)
    {
        const auto it = m_Transforms.find(entity);
        return it != m_Transforms.end() ? &it->second : nullptr;
    }

    const TransformComponent *Scene::transform(Entity entity) const
    {
        const auto it = m_Transforms.find(entity);
        return it != m_Transforms.end() ? &it->second : nullptr;
    }
}
