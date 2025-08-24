#ifndef STATICMESH_H
#define STATICMESH_H
// StaticMesh.h

// #include <glad/glad.h>
// #include <vector>
// #include "Material.h"
// #include <glm/glm.hpp>
// #include <glm/gtc/type_ptr.hpp>

// class StaticMesh {
// public:
//     unsigned int VAO, VBO;
//     unsigned int vertexCount;
//     Material* material;

//     StaticMesh(const std::vector<float>& vertices, Material* mat)
//         : material(mat) {
//         glGenVertexArrays(1, &VAO);
//         glGenBuffers(1, &VBO);

//         glBindVertexArray(VAO);
//         glBindBuffer(GL_ARRAY_BUFFER, VBO);
//         glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

//         glEnableVertexAttribArray(0);
//         glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
//         glEnableVertexAttribArray(1);
//         glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
//         glEnableVertexAttribArray(2);
//         glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

//         glBindVertexArray(0);
//         vertexCount = vertices.size() / 8;
//     }

//     ~StaticMesh() {
//         glDeleteVertexArrays(1, &VAO);
//         glDeleteBuffers(1, &VBO);
//     }

//     void Draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const {
//         material->use(model, view, projection, lightSpaceMatrix);

//         glBindVertexArray(VAO);
//         glDrawArrays(GL_TRIANGLES, 0, vertexCount);
//         glBindVertexArray(0);
//     }
// };

#pragma once

#include <glad/glad.h>
#include <vector>
#include "Material.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class StaticMesh {
public:
    unsigned int VAO, VBO, EBO; // Pridávame EBO
    unsigned int vertexCount;
    Material* material;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    // Nový konštruktor, ktorý prijíma vertexy AJ indexy
    StaticMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, Material* mat)
        : material(mat) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO); // Vygenerujeme EBO

        glBindVertexArray(VAO);

        // VBO pre dáta
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        // EBO pre indexy
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

        vertexCount = indices.size(); // Počet indexov, nie vertexov
    }
    StaticMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, const std::vector<Texture>& textures) {
        this->indices = indices;
        this->textures = textures;

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(unsigned int), &this->indices[0], GL_STATIC_DRAW);

        // layout (location = 0) - pozice
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);
        // layout (location = 1) - normaly
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));
        // layout (location = 2) - UV
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(6 * sizeof(float)));
        // layout (location = 3) - tangenta
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(8 * sizeof(float)));
        // layout (location = 4) - bitangenta
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(11 * sizeof(float)));

        glBindVertexArray(0);
    }
    ~StaticMesh() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO); // A nezabudnúť na EBO!
    }

    // Metóda Draw pre indexované vykreslenie
    void Draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& lightSpaceMatrix) const {
        material->use(model, view, projection, lightSpaceMatrix);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0); // Používame glDrawElements
        glBindVertexArray(0);
    }
    void Draw(unsigned int shaderID)
    {
        glUseProgram(shaderID); // Musíme aktivovat shader před nastavením uniform
        unsigned int diffuseNr = 1;
        unsigned int normalNr = 1;

        for (unsigned int i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            std::string number;
            std::string name = textures[i].type;
            if (name == "texture_diffuse") number = std::to_string(diffuseNr++);
            else if (name == "texture_normal") number = std::to_string(normalNr++);

            // Zde použijte shaderID, který je předán jako parametr
            glUniform1i(glGetUniformLocation(shaderID, (name + number).c_str()), i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
    }
};

#endif // STATICMESH_H
