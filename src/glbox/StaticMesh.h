#ifndef STATICMESH_H
#define STATICMESH_H

#pragma once

#include <glad/glad.h>
#include <vector>
#include "Material.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class StaticMesh {

public:

    unsigned int VAO, VBO, EBO;
    unsigned int vertexCount;

    std::vector<unsigned int> indices;
    std::vector<float> vertices;
    std::vector<Texture> textures;
     Material* material;

    StaticMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, Material* mat)
        : material(mat) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO); // EBO

        glBindVertexArray(VAO);

        // VBO
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        // EBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // Atribúty verte
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

        glBindVertexArray(0);

        vertexCount = indices.size(); //
    }

    ~StaticMesh() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO); //  EBO!
    }


    void Draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const {
        material->use(model, view, projection, lightSpaceMatrix);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

};

#endif // STATICMESH_H
