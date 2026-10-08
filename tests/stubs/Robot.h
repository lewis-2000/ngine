#pragma once

#include <string>
#include <unordered_map>

struct UrdfJointLimit
{
    bool has_limit = false;
    double lower = 0.0;
    double upper = 0.0;
};

struct UrdfJoint
{
    enum class Type
    {
        FIXED,
        REVOLUTE
    };

    std::string name;
    Type type = Type::REVOLUTE;
    UrdfJointLimit limit;
};

struct UrdfRobot
{
    std::unordered_map<std::string, UrdfJoint> joints;
};

class Robot
{
public:
    const UrdfRobot &data() const { return m_Data; }
    float jointPosition(const std::string &) const { return 0.0f; }
    void setJointPosition(const std::string &, float) {}
    const UrdfJoint *jointForChildLink(const std::string &) const
    {
        return nullptr;
    }

private:
    UrdfRobot m_Data;
};
