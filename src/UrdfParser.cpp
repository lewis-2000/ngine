#include "UrdfParser.hpp"
#include <tinyxml2.h>
#include <sstream>
#include <cmath>
#include <filesystem>

using namespace tinyxml2;

// ---------------------------------------------------------------------------
// Small parsing helpers
// ---------------------------------------------------------------------------

namespace {

// Parses "1.0 2.0 3.0" style whitespace-separated floats into a glm::vec3.
// Missing/malformed input yields (0,0,0) rather than throwing — URDF files
// in the wild are inconsistent about optional attributes, so we stay lenient
// and let the caller notice if a value looks wrong.
glm::vec3 ParseVec3(const char* str, const glm::vec3& fallback = glm::vec3(0.0f)) {
    if (!str) return fallback;
    std::istringstream iss(str);
    glm::vec3 v = fallback;
    iss >> v.x >> v.y >> v.z;
    return v;
}

double ParseDoubleAttr(const XMLElement* elem, const char* attr, double fallback = 0.0) {
    if (!elem) return fallback;
    double val = fallback;
    elem->QueryDoubleAttribute(attr, &val);
    return val;
}

UrdfOrigin ParseOrigin(const XMLElement* parent) {
    UrdfOrigin origin;
    if (!parent) return origin;
    const XMLElement* origin_elem = parent->FirstChildElement("origin");
    if (!origin_elem) return origin; // URDF default is identity if omitted
    origin.xyz = ParseVec3(origin_elem->Attribute("xyz"));
    origin.rpy = ParseVec3(origin_elem->Attribute("rpy"));
    return origin;
}

UrdfGeometry ParseGeometry(const XMLElement* visual_or_collision_elem) {
    UrdfGeometry geom;
    const XMLElement* geom_elem = visual_or_collision_elem->FirstChildElement("geometry");
    if (!geom_elem) return geom;

    if (const XMLElement* box = geom_elem->FirstChildElement("box")) {
        geom.type = UrdfGeometry::Type::BOX;
        geom.box_size = ParseVec3(box->Attribute("size"), glm::vec3(1.0f));
    } else if (const XMLElement* cyl = geom_elem->FirstChildElement("cylinder")) {
        geom.type = UrdfGeometry::Type::CYLINDER;
        geom.cylinder_radius = static_cast<float>(ParseDoubleAttr(cyl, "radius"));
        geom.cylinder_length = static_cast<float>(ParseDoubleAttr(cyl, "length"));
    } else if (const XMLElement* sph = geom_elem->FirstChildElement("sphere")) {
        geom.type = UrdfGeometry::Type::SPHERE;
        geom.sphere_radius = static_cast<float>(ParseDoubleAttr(sph, "radius"));
    } else if (const XMLElement* mesh = geom_elem->FirstChildElement("mesh")) {
        geom.type = UrdfGeometry::Type::MESH;
        if (const char* fn = mesh->Attribute("filename")) geom.mesh_filename = fn;
        geom.mesh_scale = ParseVec3(mesh->Attribute("scale"), glm::vec3(1.0f));
    }
    return geom;
}

UrdfVisual ParseVisual(const XMLElement* visual_elem) {
    UrdfVisual visual;
    visual.origin = ParseOrigin(visual_elem);
    visual.geometry = ParseGeometry(visual_elem);
    if (const XMLElement* material = visual_elem->FirstChildElement("material")) {
        if (const char* name = material->Attribute("name")) visual.material_name = name;
        if (const XMLElement* color = material->FirstChildElement("color")) {
            // rgba packs 4 values; ParseVec3 only reads 3, so parse all 4 directly here
            std::istringstream iss(color->Attribute("rgba") ? color->Attribute("rgba") : "");
            float r, g, b, a = 1.0f;
            iss >> r >> g >> b >> a;
            visual.material_color = glm::vec4(r, g, b, a);
        }
    }
    return visual;
}

UrdfCollision ParseCollision(const XMLElement* collision_elem) {
    UrdfCollision collision;
    collision.origin = ParseOrigin(collision_elem);
    collision.geometry = ParseGeometry(collision_elem);
    return collision;
}

UrdfInertial ParseInertial(const XMLElement* link_elem) {
    UrdfInertial inertial;
    const XMLElement* inertial_elem = link_elem->FirstChildElement("inertial");
    if (!inertial_elem) return inertial; // mass 0 signals "not specified"

    inertial.origin = ParseOrigin(inertial_elem);
    if (const XMLElement* mass_elem = inertial_elem->FirstChildElement("mass")) {
        inertial.mass = ParseDoubleAttr(mass_elem, "value");
    }
    if (const XMLElement* i = inertial_elem->FirstChildElement("inertia")) {
        inertial.ixx = ParseDoubleAttr(i, "ixx");
        inertial.ixy = ParseDoubleAttr(i, "ixy");
        inertial.ixz = ParseDoubleAttr(i, "ixz");
        inertial.iyy = ParseDoubleAttr(i, "iyy");
        inertial.iyz = ParseDoubleAttr(i, "iyz");
        inertial.izz = ParseDoubleAttr(i, "izz");
    }
    return inertial;
}

UrdfJoint::Type ParseJointType(const char* type_str) {
    if (!type_str) return UrdfJoint::Type::FIXED;
    std::string t = type_str;
    if (t == "revolute") return UrdfJoint::Type::REVOLUTE;
    if (t == "continuous") return UrdfJoint::Type::CONTINUOUS;
    if (t == "prismatic") return UrdfJoint::Type::PRISMATIC;
    if (t == "floating") return UrdfJoint::Type::FLOATING;
    if (t == "planar") return UrdfJoint::Type::PLANAR;
    return UrdfJoint::Type::FIXED;
}

} // namespace

// ---------------------------------------------------------------------------
// UrdfRobot
// ---------------------------------------------------------------------------

std::vector<const UrdfJoint*> UrdfRobot::GetChildJoints(const std::string& link_name) const {
    std::vector<const UrdfJoint*> result;
    for (const auto& [name, joint] : joints) {
        if (joint.parent_link == link_name) result.push_back(&joint);
    }
    return result;
}

std::string UrdfRobot::FindRootLink() const {
    // A root link is one that never appears as a child_link. Collect all
    // child links first, then the root is whichever link isn't in that set.
    std::unordered_map<std::string, bool> is_child;
    for (const auto& [name, link] : links) is_child[name] = false;
    for (const auto& [name, joint] : joints) is_child[joint.child_link] = true;

    std::string root;
    int root_count = 0;
    for (const auto& [name, was_child] : is_child) {
        if (!was_child) {
            root = name;
            root_count++;
        }
    }
    // Exactly one root is the well-formed case; 0 or >1 indicates a cycle
    // or multiple disconnected trees, which we surface as "not found" and
    // let the caller decide how to handle rather than guessing.
    return (root_count == 1) ? root : "";
}

// ---------------------------------------------------------------------------
// UrdfParser
// ---------------------------------------------------------------------------

bool UrdfParser::ParseFile(const std::string& path, UrdfRobot& out_robot) {
    if (!std::filesystem::exists(path)) {
        last_error_ = "File does not exist: " + path;
        return false;
    }

    XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != XML_SUCCESS) {
        last_error_ = "XML parse error in " + path + ": " +
                      (doc.ErrorStr() ? doc.ErrorStr() : "unknown error");
        return false;
    }

    const XMLElement* robot_elem = doc.RootElement();
    if (!robot_elem || std::string(robot_elem->Name()) != "robot") {
        last_error_ = "Root element is not <robot> in " + path;
        return false;
    }
    return ParseRobotElement(robot_elem, out_robot);
}

bool UrdfParser::ParseString(const std::string& xml_content, UrdfRobot& out_robot) {
    XMLDocument doc;
    if (doc.Parse(xml_content.c_str(), xml_content.size()) != XML_SUCCESS) {
        last_error_ = std::string("XML parse error: ") +
                      (doc.ErrorStr() ? doc.ErrorStr() : "unknown error");
        return false;
    }

    const XMLElement* robot_elem = doc.RootElement();
    if (!robot_elem || std::string(robot_elem->Name()) != "robot") {
        last_error_ = "Root element is not <robot>";
        return false;
    }
    return ParseRobotElement(robot_elem, out_robot);
}

bool UrdfParser::ParseRobotElement(const XMLElement* robot_elem, UrdfRobot& out_robot) {
    out_robot.links.clear();
    out_robot.joints.clear();

    if (const char* name = robot_elem->Attribute("name")) out_robot.name = name;

    // --- Links ---
    for (const XMLElement* link_elem = robot_elem->FirstChildElement("link");
         link_elem; link_elem = link_elem->NextSiblingElement("link")) {
        UrdfLink link;
        const char* name = link_elem->Attribute("name");
        if (!name) {
            last_error_ = "Found <link> with no name attribute";
            return false;
        }
        link.name = name;
        link.inertial = ParseInertial(link_elem);

        for (const XMLElement* visual_elem = link_elem->FirstChildElement("visual");
             visual_elem; visual_elem = visual_elem->NextSiblingElement("visual")) {
            link.visuals.push_back(ParseVisual(visual_elem));
        }
        for (const XMLElement* collision_elem = link_elem->FirstChildElement("collision");
             collision_elem; collision_elem = collision_elem->NextSiblingElement("collision")) {
            link.collisions.push_back(ParseCollision(collision_elem));
        }

        out_robot.links[link.name] = std::move(link);
    }

    // --- Joints ---
    for (const XMLElement* joint_elem = robot_elem->FirstChildElement("joint");
         joint_elem; joint_elem = joint_elem->NextSiblingElement("joint")) {
        UrdfJoint joint;
        const char* name = joint_elem->Attribute("name");
        if (!name) {
            last_error_ = "Found <joint> with no name attribute";
            return false;
        }
        joint.name = name;
        joint.type = ParseJointType(joint_elem->Attribute("type"));
        joint.origin = ParseOrigin(joint_elem);

        const XMLElement* parent_elem = joint_elem->FirstChildElement("parent");
        const XMLElement* child_elem = joint_elem->FirstChildElement("child");
        if (!parent_elem || !parent_elem->Attribute("link") ||
            !child_elem || !child_elem->Attribute("link")) {
            last_error_ = "Joint '" + joint.name + "' is missing <parent>/<child> link attribute";
            return false;
        }
        joint.parent_link = parent_elem->Attribute("link");
        joint.child_link = child_elem->Attribute("link");

        if (const XMLElement* axis_elem = joint_elem->FirstChildElement("axis")) {
            joint.axis = ParseVec3(axis_elem->Attribute("xyz"), glm::vec3(1.0f, 0.0f, 0.0f));
        }

        if (const XMLElement* limit_elem = joint_elem->FirstChildElement("limit")) {
            joint.limit.has_limit = true;
            joint.limit.lower = ParseDoubleAttr(limit_elem, "lower");
            joint.limit.upper = ParseDoubleAttr(limit_elem, "upper");
            joint.limit.effort = ParseDoubleAttr(limit_elem, "effort");
            joint.limit.velocity = ParseDoubleAttr(limit_elem, "velocity");
        }

        // Sanity check: both ends of the joint should reference links we
        // actually parsed. Missing links usually means the URDF references
        // a link defined via xacro macro expansion that didn't run, or a
        // typo — worth surfacing rather than silently building a broken tree.
        if (out_robot.links.find(joint.parent_link) == out_robot.links.end()) {
            last_error_ = "Joint '" + joint.name + "' references unknown parent link '" +
                          joint.parent_link + "'";
            return false;
        }
        if (out_robot.links.find(joint.child_link) == out_robot.links.end()) {
            last_error_ = "Joint '" + joint.name + "' references unknown child link '" +
                          joint.child_link + "'";
            return false;
        }

        out_robot.joints[joint.name] = std::move(joint);
    }

    if (out_robot.FindRootLink().empty() && out_robot.links.size() > 1) {
        last_error_ = "Could not find a unique root link — check for cycles or "
                      "disconnected trees in the joint graph";
        return false;
    }

    return true;
}

void UrdfParser::RegisterPackagePath(const std::string& package_name, const std::string& directory) {
    package_paths_[package_name] = directory;
}

std::string UrdfParser::ResolveMeshPath(const std::string& urdf_mesh_filename) const {
    const std::string prefix = "package://";
    if (urdf_mesh_filename.rfind(prefix, 0) != 0) {
        return urdf_mesh_filename; // not a package:// URI — return as-is
    }

    std::string remainder = urdf_mesh_filename.substr(prefix.size());
    size_t slash_pos = remainder.find('/');
    if (slash_pos == std::string::npos) {
        return urdf_mesh_filename; // malformed URI, nothing sensible to resolve
    }

    std::string package_name = remainder.substr(0, slash_pos);
    std::string relative_path = remainder.substr(slash_pos + 1);

    auto it = package_paths_.find(package_name);
    if (it == package_paths_.end()) {
        return urdf_mesh_filename; // unknown package — caller should check
                                    // GetLastError()/log this case themselves
    }

    std::filesystem::path resolved = std::filesystem::path(it->second) / relative_path;
    return resolved.string();
}
