#ifndef STATICMESH_H
#define STATICMESH_H
// StaticMesh.h

#include <glad/glad.h>
#include <vector>
#include "Material.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class StaticMesh {
public:
    unsigned int VAO, VBO;
    unsigned int vertexCount;
    Material* material;

    StaticMesh(const std::vector<float>& vertices, Material* mat)
        : material(mat) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

        glBindVertexArray(0);
        vertexCount = vertices.size() / 8;
    }

    ~StaticMesh() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void Draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const {
        material->use(model, view, projection, lightSpaceMatrix);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glBindVertexArray(0);
    }
};
#endif // STATICMESH_H
