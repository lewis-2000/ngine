#include "Renderer.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Mara
{
    Renderer::Renderer(Window &window, Editor &editor, Robot &robot, Plane &plane)
        : m_Window(window), m_Editor(editor), m_Robot(robot), m_Plane(plane)
    {
    }

    void Renderer::initialize()
    {
        m_Shader = std::make_unique<Shader>("shaders/scene.vert", "shaders/scene.frag");
    }

    void Renderer::render(
        const Scene &scene,
        Entity robotEntity,
        Entity groundEntity,
        const Camera &camera)
    {
        m_Editor.beginSceneRender(m_Window.getWidth(), m_Window.getHeight());
        m_Shader->use();

        const TransformComponent *robotTransform = scene.transform(robotEntity);
        const TransformComponent *groundTransform = scene.transform(groundEntity);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), robotTransform->position);
        model = glm::rotate(model, robotTransform->rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, robotTransform->rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, robotTransform->rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, robotTransform->scale);

        const glm::vec3 cameraPosition = camera.position();
        m_Shader->setMat4("model", model);
        m_Shader->setMat4("view", camera.viewMatrix());
        m_Shader->setMat4(
            "projection",
            camera.projectionMatrix(m_Window.getWidth(), m_Window.getHeight()));
        m_Shader->setVec3("viewPos", cameraPosition);
        m_Shader->setVec3("lightDirection", -1.0f, -1.0f, -1.0f);
        m_Shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        m_Shader->setVec3("ambientColor", 1.0f, 1.0f, 1.0f);
        m_Shader->setFloat("ambientStrength", 0.3f);

        glm::mat4 groundMatrix = glm::translate(
            glm::mat4(1.0f), groundTransform->position);
        groundMatrix = glm::rotate(
            groundMatrix,
            groundTransform->rotation.x,
            glm::vec3(1.0f, 0.0f, 0.0f));
        groundMatrix = glm::scale(groundMatrix, groundTransform->scale);
        m_Plane.Draw(*m_Shader, groundMatrix);
        m_Robot.Draw(*m_Shader, model);

        m_Editor.endSceneRender();
    }
}
