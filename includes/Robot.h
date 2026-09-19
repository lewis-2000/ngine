#pragma once

#include "Model.h"
#include "UrdfParser.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>

class Robot
{
public:
    bool LoadFromUrdf(const std::filesystem::path &urdfPath);

    bool isLoaded() const { return m_Loaded; }
    const std::string &name() const { return m_Robot.name; }
    const std::string &rootLink() const { return m_RootLink; }
    const std::string &lastError() const { return m_LastError; }
    const UrdfRobot &data() const { return m_Robot; }

    std::filesystem::path resolveMeshPath(const std::string &meshFilename) const;
    void Draw(Shader &shader, const glm::mat4 &robotTransform) const;

private:
    struct LoadedVisual
    {
        std::unique_ptr<Model> model;
        glm::mat4 transform{1.0f};
    };

    using LinkModels = std::vector<LoadedVisual>;

    glm::mat4 linkTransform(const std::string &linkName) const;
    static glm::mat4 originTransform(const UrdfOrigin &origin);

    UrdfParser m_Parser;
    UrdfRobot m_Robot;
    std::unordered_map<std::string, LinkModels> m_LinkModels;
    std::filesystem::path m_UrdfDirectory;
    std::string m_RootLink;
    std::string m_LastError;
    bool m_Loaded = false;
};