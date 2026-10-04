#include "Renderer.h"

#include <cmath>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace Mara
{
    Renderer::Renderer(Window &window, Editor &editor, Robot &robot, Plane &plane)
        : m_Window(window), m_Editor(editor), m_Robot(robot), m_Plane(plane)
    {
    }

    Renderer::~Renderer()
    {
        if (m_SensorVbo != 0)
            glDeleteBuffers(1, &m_SensorVbo);
        if (m_SensorVao != 0)
            glDeleteVertexArrays(1, &m_SensorVao);
    }

    void Renderer::initialize()
    {
        m_Shader = std::make_unique<Shader>("shaders/scene.vert", "shaders/scene.frag");
        m_AxisShader = std::make_unique<Shader>("shaders/axis.vert", "shaders/axis.frag");

        std::vector<float> sensorVertices;
        constexpr int rayCount = 24;
        for (int ray = 0; ray < rayCount; ++ray)
        {
            const float angle = -1.1f + 2.2f * static_cast<float>(ray) / (rayCount - 1);
            sensorVertices.insert(sensorVertices.end(), {
                0.0f, 0.0f, 0.0f, 0.15f, 0.85f, 0.95f,
                2.0f * std::sin(angle), 0.0f, 2.0f * std::cos(angle),
                0.15f, 0.85f, 0.95f});
        }
        glGenVertexArrays(1, &m_SensorVao);
        glGenBuffers(1, &m_SensorVbo);
        glBindVertexArray(m_SensorVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_SensorVbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(sensorVertices.size() * sizeof(float)),
            sensorVertices.data(),
            GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
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
            camera.projectionMatrix(
                m_Editor.sceneWidth(),
                m_Editor.sceneHeight()));
        m_Shader->setVec3("viewPos", cameraPosition);
        m_Shader->setVec3("lightDirection", -1.0f, -1.0f, -1.0f);
        m_Shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        m_Shader->setVec3("ambientColor", 1.0f, 1.0f, 1.0f);
        m_Shader->setFloat("ambientStrength", 0.3f);
        m_Editor.setGizmoMatrices(
            camera.viewMatrix(),
            camera.projectionMatrix(m_Editor.sceneWidth(), m_Editor.sceneHeight()),
            model);

        glm::mat4 groundMatrix = glm::translate(
            glm::mat4(1.0f), groundTransform->position);
        groundMatrix = glm::rotate(
            groundMatrix,
            groundTransform->rotation.x,
            glm::vec3(1.0f, 0.0f, 0.0f));
        groundMatrix = glm::scale(groundMatrix, groundTransform->scale);
        m_Shader->setBool("uSelectedLink", false);
        m_Plane.Draw(*m_Shader, groundMatrix, m_Editor.showGrid());
        m_Robot.Draw(*m_Shader, model, m_Editor.selectedLink());

        const std::string &selectedLink = m_Editor.selectedLink();
        const bool sensorLink =
            selectedLink.find("sensor") != std::string::npos ||
            selectedLink.find("lidar") != std::string::npos ||
            selectedLink.find("camera") != std::string::npos;
        if (m_Editor.showSensors() && sensorLink)
        {
            const glm::mat4 sensorTransform =
                m_Robot.linkWorldTransform(selectedLink, model);
            m_AxisShader->use();
            m_AxisShader->setMat4("view", camera.viewMatrix());
            m_AxisShader->setMat4(
                "projection",
                camera.projectionMatrix(m_Editor.sceneWidth(), m_Editor.sceneHeight()));
            m_AxisShader->setMat4("model", sensorTransform);
            glBindVertexArray(m_SensorVao);
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, 2 * 24);
            glBindVertexArray(0);
            m_Shader->use();
        }

        m_Editor.endSceneRender();
    }
}
