#include "Plane.h"

#include <utility>

Plane::Plane(float size)
    : m_Mesh(
          [&]()
          {
              const float halfSize = size * 0.5f;
              const float spacing = 0.25f;
              const float lineWidth = 0.0125f;
              std::vector<Vertex> vertices;
              std::vector<GLuint> indices;

              auto addQuad = [&](float x0, float z0, float x1, float z1)
              {
                  const GLuint first = static_cast<GLuint>(vertices.size());
                  vertices.insert(vertices.end(), {
                                                      {{x0, 0.0f, z0}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                                                      {{x1, 0.0f, z0}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                                      {{x1, 0.0f, z1}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
                                                      {{x0, 0.0f, z1}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
                                                  });
                  indices.insert(indices.end(), {first, first + 2, first + 1,
                                                 first, first + 3, first + 2});
              };

              for (float coordinate = -halfSize; coordinate <= halfSize; coordinate += spacing)
              {
                  addQuad(coordinate - lineWidth, -halfSize,
                          coordinate + lineWidth, halfSize);
                  addQuad(-halfSize, coordinate - lineWidth,
                          halfSize, coordinate + lineWidth);
              }

              return Mesh(std::move(vertices), std::move(indices), {}, "ground_grid", true);
          }()),
      m_Material{MeshMaterialOverride::Mode::Color, glm::vec3(0.30f, 0.34f, 0.36f), {}}
{
}

void Plane::Draw(Shader &shader, const glm::mat4 &transform)
{
    shader.setMat4("model", transform);
    m_Mesh.Draw(shader, &m_Material);
}