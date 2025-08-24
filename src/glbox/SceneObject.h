#ifndef SCENEOBJECT_H
#define SCENEOBJECT_H

#include "Transform.h"
#include "StaticMesh.h"

class SceneObject {
public:
    Transform transform;
    StaticMesh* mesh;

    SceneObject(StaticMesh* mesh) : mesh(mesh) {}

    void Draw(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const {
        mesh->Draw(transform.GetModelMatrix(), view, projection, lightSpaceMatrix);
    }

    void DrawForShadow(unsigned int depthShaderID, const glm::mat4& lightSpaceMatrix) const {
        glUseProgram(depthShaderID);
        glUniformMatrix4fv(glGetUniformLocation(depthShaderID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glUniformMatrix4fv(glGetUniformLocation(depthShaderID, "model"), 1, GL_FALSE, glm::value_ptr(transform.GetModelMatrix()));

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->vertexCount, GL_UNSIGNED_INT, 0); // Používame glDrawElements
        glBindVertexArray(0);
    }
};
#endif // SCENEOBJECT_H
