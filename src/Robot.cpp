#include "Robot.h"

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cmath>

bool Robot::LoadFromUrdf(const std::filesystem::path &urdfPath)
{
    m_Loaded = false;
    m_LastError.clear();
    m_RootLink.clear();
    m_Robot = {};
    m_LinkModels.clear();
    m_JointPositions.clear();

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

    for (const auto &[jointName, joint] : m_Robot.joints)
        m_JointPositions[jointName] = 0.0f;

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

void Robot::setJointPosition(const std::string &jointName, float position)
{
    const auto jointIt = m_Robot.joints.find(jointName);
    if (jointIt == m_Robot.joints.end())
        return;

    const UrdfJoint &joint = jointIt->second;
    if (joint.limit.has_limit)
    {
        position = glm::clamp(
            position,
            static_cast<float>(joint.limit.lower),
            static_cast<float>(joint.limit.upper));
    }

    m_JointPositions[jointName] = position;
}

float Robot::jointPosition(const std::string &jointName) const
{
    const auto positionIt = m_JointPositions.find(jointName);
    return positionIt != m_JointPositions.end() ? positionIt->second : 0.0f;
}

const UrdfJoint *Robot::jointForChildLink(const std::string &linkName) const
{
    for (const auto &[jointName, joint] : m_Robot.joints)
    {
        if (joint.child_link == linkName)
            return &joint;
    }

        return nullptr;
    }

    bool Robot::setJointAxis(const std::string &jointName, const glm::vec3 &axis)
    {
        const auto jointIt = m_Robot.joints.find(jointName);
        if (jointIt == m_Robot.joints.end() || glm::length(axis) <= 0.0001f)
            return false;

        jointIt->second.axis = glm::normalize(axis);
        return true;
}

void Robot::resetJointPositions()
{
    for (const auto &[jointName, joint] : m_Robot.joints)
        setJointPosition(jointName, 0.0f);
}

void Robot::updateDemoAnimation(float elapsedTime)
{
    for (const auto &[jointName, joint] : m_Robot.joints)
    {
        if (joint.type != UrdfJoint::Type::FIXED)
            updateDemoAnimation(elapsedTime, joint.child_link);
    }
}

void Robot::updateDemoAnimation(
    float elapsedTime,
    const std::string &selectedLink)
{
    const UrdfJoint *joint = jointForChildLink(selectedLink);
    if (!joint || joint->type == UrdfJoint::Type::FIXED)
        return;

    float amplitude = 0.5f;
    if (joint->limit.has_limit)
    {
        const float lower = static_cast<float>(joint->limit.lower);
        const float upper = static_cast<float>(joint->limit.upper);
        amplitude = (upper - lower) * 0.5f;
        setJointPosition(
            joint->name,
            (lower + upper) * 0.5f + amplitude * std::sin(elapsedTime));
    }
    else
    {
        setJointPosition(joint->name, amplitude * std::sin(elapsedTime));
    }
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
        {
            glm::mat4 transform =
                linkTransform(joint.parent_link) * originTransform(joint.origin);
            const float position = jointPosition(jointName);

            if (joint.type == UrdfJoint::Type::REVOLUTE ||
                joint.type == UrdfJoint::Type::CONTINUOUS)
            {
                const glm::vec3 axis = glm::length(joint.axis) > 0.0f
                                           ? glm::normalize(joint.axis)
                                           : glm::vec3(1.0f, 0.0f, 0.0f);
                transform = glm::rotate(
                    transform,
                    position,
                    axis);
            }
            else if (joint.type == UrdfJoint::Type::PRISMATIC)
            {
                const glm::vec3 axis = glm::length(joint.axis) > 0.0f
                                           ? glm::normalize(joint.axis)
                                           : glm::vec3(1.0f, 0.0f, 0.0f);
                transform = glm::translate(
                    transform,
                    axis * position);
            }

            return transform;
        }
    }

    return glm::mat4(1.0f);
}

void Robot::Draw(
    Shader &shader,
    const glm::mat4 &robotTransform,
    const std::string &selectedLink) const
{
    if (!m_Loaded)
        return;

    for (const auto &[linkName, models] : m_LinkModels)
    {
        const glm::mat4 transform =
            robotTransform * linkTransform(linkName);
        shader.setMat4("model", transform);
        shader.setBool("uSelectedLink", linkName == selectedLink);

        for (const LoadedVisual &visual : models)
        {
            shader.setMat4("model", transform * visual.transform);
            visual.model->Draw(shader);
        }
    }
}

glm::mat4 Robot::linkWorldTransform(
    const std::string &linkName,
    const glm::mat4 &robotTransform) const
{
    return robotTransform * linkTransform(linkName);
}

glm::mat4 Robot::jointFrameWorldTransform(
    const std::string &linkName,
    const glm::mat4 &robotTransform) const
{
    const UrdfJoint *joint = jointForChildLink(linkName);
    if (!joint)
        return linkWorldTransform(linkName, robotTransform);

    return robotTransform * linkTransform(joint->parent_link) * originTransform(joint->origin);
}

bool Robot::setJointFrameWorldTransform(
    const std::string &linkName,
    const glm::mat4 &robotTransform,
    const glm::mat4 &worldTransform)
{
    const UrdfJoint *joint = jointForChildLink(linkName);
    if (!joint)
        return false;

    const glm::mat4 parentWorld =
        robotTransform * linkTransform(joint->parent_link);
    const glm::mat4 localTransform =
        glm::inverse(parentWorld) * worldTransform;

    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    if (!glm::decompose(
            localTransform,
            scale,
            rotation,
            translation,
            skew,
            perspective))
    {
        return false;
    }

    auto jointIt = m_Robot.joints.find(joint->name);
    if (jointIt == m_Robot.joints.end())
        return false;

    jointIt->second.origin.xyz = translation;
    jointIt->second.origin.rpy = glm::eulerAngles(rotation);
    return true;
}