#include "Robot.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

bool Robot::LoadFromUrdf(const std::filesystem::path &urdfPath)
{
    m_Loaded = false;
    m_LastError.clear();
    m_RootLink.clear();
    m_Robot = {};
    m_LinkModels.clear();

    if (!m_Parser.ParseFile(urdfPath.string(), m_Robot))
    {
        m_LastError = m_Parser.GetLastError();
        return false;
    }

    m_UrdfDirectory = urdfPath.parent_path();
    m_RootLink = m_Robot.FindRootLink();
    if (m_RootLink.empty() && m_Robot.links.size() > 1)
    {
        m_LastError = "URDF does not contain a unique root link";
        return false;
    }

    for (const auto &[linkName, link] : m_Robot.links)
    {
        for (const UrdfVisual &visual : link.visuals)
        {
            if (visual.geometry.type != UrdfGeometry::Type::MESH)
                continue;

            const std::filesystem::path meshPath =
                resolveMeshPath(visual.geometry.mesh_filename);
            if (!std::filesystem::exists(meshPath))
            {
                m_LastError = "Mesh file does not exist: " + meshPath.string();
                return false;
            }

            auto model = std::make_unique<Model>(meshPath.string());
            if (model->meshCount() == 0)
            {
                m_LastError = "Could not load mesh: " + meshPath.string();
                return false;
            }

            LoadedVisual loadedVisual;
            loadedVisual.model = std::move(model);
            loadedVisual.transform = originTransform(visual.origin);
            loadedVisual.transform = glm::scale(
                loadedVisual.transform,
                visual.geometry.mesh_scale);
            m_LinkModels[linkName].push_back(std::move(loadedVisual));
        }
    }

    m_Loaded = true;
    return true;
}

std::filesystem::path Robot::resolveMeshPath(const std::string &meshFilename) const
{
    const std::string resolved = m_Parser.ResolveMeshPath(meshFilename);
    const std::filesystem::path meshPath(resolved);

    if (meshPath.is_absolute())
        return meshPath;

    return (m_UrdfDirectory / meshPath).lexically_normal();
}

glm::mat4 Robot::originTransform(const UrdfOrigin &origin)
{
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, origin.xyz);
    transform = glm::rotate(transform, origin.rpy.z, glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::rotate(transform, origin.rpy.y, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::rotate(transform, origin.rpy.x, glm::vec3(1.0f, 0.0f, 0.0f));
    return transform;
}

glm::mat4 Robot::linkTransform(const std::string &linkName) const
{
    for (const auto &[jointName, joint] : m_Robot.joints)
    {
        if (joint.child_link == linkName)
            return linkTransform(joint.parent_link) * originTransform(joint.origin);
    }

    return glm::mat4(1.0f);
}

void Robot::Draw(Shader &shader, const glm::mat4 &robotTransform) const
{
    if (!m_Loaded)
        return;

    for (const auto &[linkName, models] : m_LinkModels)
    {
        const glm::mat4 transform =
            robotTransform * linkTransform(linkName);
        shader.setMat4("model", transform);

        for (const LoadedVisual &visual : models)
        {
            shader.setMat4("model", transform * visual.transform);
            visual.model->Draw(shader);
        }
    }
}