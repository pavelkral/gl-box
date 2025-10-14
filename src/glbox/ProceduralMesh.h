#ifndef PROCEDURALMESH_H
#define PROCEDURALMESH_H

#include <vector>
#include "Material.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class ProceduralMesh {
public:
    unsigned int VAO, VBO, EBO;
    unsigned int vertexCount;

    std::vector<unsigned int> indices;
    std::vector<float> vertices;
    std::vector<Texture> textures;
    Material* material;

    ProceduralMesh(const std::vector<float>& vertices,
               const std::vector<unsigned int>& indices,
               Material* mat)
        : vertices(vertices), indices(indices), material(mat)
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        // VBO
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        // EBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        // Atribúty verte
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindVertexArray(0);
        vertexCount = indices.size();
    }

    ~ProceduralMesh() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }


    void UpdateGeometry(const std::vector<float>& newVertices,
                        const std::vector<unsigned int>& newIndices)
    {
        vertices = newVertices;
        indices = newIndices;
        vertexCount = static_cast<unsigned int>(indices.size());

        glBindVertexArray(VAO);
        // Update VBO dat
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        // Update EBO dat
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(0);
    }

    void Draw(const glm::mat4& model, const glm::mat4& view,
              const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const
    {
        material->use(model, view, projection, lightSpaceMatrix);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

#endif // PROCEDURALMESH_H


