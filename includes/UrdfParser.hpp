#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

// Forward declaration so this header doesn't force every consumer to also
// include tinyxml2.h — only UrdfParser.cpp needs the full definition.
namespace tinyxml2 { class XMLElement; }

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct UrdfOrigin {
    glm::vec3 xyz{0.0f, 0.0f, 0.0f};
    glm::vec3 rpy{0.0f, 0.0f, 0.0f}; // roll, pitch, yaw (radians)
};

struct UrdfGeometry {
    enum class Type { BOX, CYLINDER, SPHERE, MESH } type = Type::BOX;
    glm::vec3 box_size{1.0f, 1.0f, 1.0f};
    float cylinder_radius = 0.0f;
    float cylinder_length = 0.0f;
    float sphere_radius = 0.0f;
    std::string mesh_filename; // may contain package:// prefix — see ResolveMeshPath
    glm::vec3 mesh_scale{1.0f, 1.0f, 1.0f};
};

struct UrdfVisual {
    UrdfOrigin origin;
    UrdfGeometry geometry;
    std::string material_name;
    glm::vec4 material_color{0.8f, 0.8f, 0.8f, 1.0f}; // only set if <color> present
};

struct UrdfCollision {
    UrdfOrigin origin;
    UrdfGeometry geometry;
};

struct UrdfInertial {
    UrdfOrigin origin;
    double mass = 0.0;
    // Symmetric 3x3 inertia tensor, upper triangle
    double ixx = 0.0, ixy = 0.0, ixz = 0.0;
    double iyy = 0.0, iyz = 0.0;
    double izz = 0.0;
};

struct UrdfLink {
    std::string name;
    UrdfInertial inertial;
    std::vector<UrdfVisual> visuals;
    std::vector<UrdfCollision> collisions;
};

struct UrdfJointLimit {
    double lower = 0.0;
    double upper = 0.0;
    double effort = 0.0;
    double velocity = 0.0;
    bool has_limit = false; // false for continuous/floating/planar joints
};

struct UrdfJoint {
    enum class Type { REVOLUTE, CONTINUOUS, PRISMATIC, FIXED, FLOATING, PLANAR } type = Type::FIXED;
    std::string name;
    std::string parent_link;
    std::string child_link;
    UrdfOrigin origin;      // pose of child frame relative to parent frame
    glm::vec3 axis{1.0f, 0.0f, 0.0f}; // meaningful for revolute/continuous/prismatic
    UrdfJointLimit limit;
};

// A parsed robot: flat maps of links/joints plus the derived tree structure.
// Links form a tree via joints; find the root by locating the link that never
// appears as a child in any joint.
struct UrdfRobot {
    std::string name;
    std::unordered_map<std::string, UrdfLink> links;   // keyed by link name
    std::unordered_map<std::string, UrdfJoint> joints;  // keyed by joint name

    // Convenience: joints whose parent_link == given link name, in document order
    std::vector<const UrdfJoint*> GetChildJoints(const std::string& link_name) const;

    // The link that is never a child of any joint. Empty string if not found
    // (malformed URDF) or if there are multiple disconnected trees.
    std::string FindRootLink() const;
};

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

class UrdfParser {
public:
    // Parses a URDF file from disk. Returns false on failure; check
    // GetLastError() for a human-readable reason.
    bool ParseFile(const std::string& path, UrdfRobot& out_robot);

    // Parses URDF XML already loaded into a string (useful for tests or
    // URDF generated in-memory, e.g. from xacro output).
    bool ParseString(const std::string& xml_content, UrdfRobot& out_robot);

    const std::string& GetLastError() const { return last_error_; }

    // Resolves a "package://pkg_name/path/to/mesh.dae" style URI into an
    // absolute filesystem path, using a simple package_name -> directory
    // lookup table you populate ahead of time (e.g. from your ROS2
    // ament_index or a manual map for non-ROS use). Returns the input
    // unchanged if it doesn't start with "package://" or the package is
    // unknown (check IsKnownPackage first if you want to detect that case).
    void RegisterPackagePath(const std::string& package_name, const std::string& directory);
    std::string ResolveMeshPath(const std::string& urdf_mesh_filename) const;

private:
    std::string last_error_;
    std::unordered_map<std::string, std::string> package_paths_;

    // Shared by ParseFile/ParseString once the XML doc is loaded.
    bool ParseRobotElement(const tinyxml2::XMLElement* robot_elem, UrdfRobot& out_robot);
};
