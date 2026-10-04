#pragma once

#include "Mesh.h"

#include <glm/mat4x4.hpp>

class Plane
{
public:
    explicit Plane(float size = 20.0f);

    void Draw(Shader &shader, const glm::mat4 &transform, bool visible = true);

private:
    Mesh m_Mesh;
    MeshMaterialOverride m_Material;
};