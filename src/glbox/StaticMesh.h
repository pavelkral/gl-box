#ifndef STATICMESH_H
#define STATICMESH_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "PbrMaterial.h"

class StaticMesh {
public:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int indexCount;

    std::vector<float> vertices;    //  data Stride 11 (P, N, UV, T)
    std::vector<unsigned int> indices;
    PbrMaterial* material;
    static constexpr int VERTEX_STRIDE = 11;
    static constexpr int INPUT_STRIDE = 8;

    static void CalculateTangents(std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
        if (vertices.empty() || indices.empty()) return;

        constexpr int INPUT_STRIDE_LOCAL = 8;
        constexpr int OUTPUT_STRIDE_LOCAL = 11;

        if (vertices.size() % INPUT_STRIDE_LOCAL != 0) {
            std::cerr << "ERR: CalculateTangents: WAIT " << INPUT_STRIDE_LOCAL << " float ON vertex (P(3), N(3), UV(2))." << std::endl;
            return;
        }

        size_t numVertices = vertices.size() / INPUT_STRIDE_LOCAL;
        std::vector<glm::vec3> tempTangents(numVertices, glm::vec3(0.0f));

        for (size_t i = 0; i < indices.size(); i += 3) {
            unsigned int i1 = indices[i], i2 = indices[i+1], i3 = indices[i+2];
            if (i1 >= numVertices || i2 >= numVertices || i3 >= numVertices) continue;

            glm::vec3 pos1 = glm::make_vec3(&vertices[i1 * INPUT_STRIDE_LOCAL + 0]);
            glm::vec3 pos2 = glm::make_vec3(&vertices[i2 * INPUT_STRIDE_LOCAL + 0]);
            glm::vec3 pos3 = glm::make_vec3(&vertices[i3 * INPUT_STRIDE_LOCAL + 0]);

            glm::vec2 uv1 = glm::make_vec2(&vertices[i1 * INPUT_STRIDE_LOCAL + 6]);
            glm::vec2 uv2 = glm::make_vec2(&vertices[i2 * INPUT_STRIDE_LOCAL + 6]);
            glm::vec2 uv3 = glm::make_vec2(&vertices[i3 * INPUT_STRIDE_LOCAL + 6]);

            glm::vec3 edge1 = pos2 - pos1;
            glm::vec3 edge2 = pos3 - pos1;
            glm::vec2 deltaUV1 = uv2 - uv1;
            glm::vec2 deltaUV2 = uv3 - uv1;

            float det = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
            float f = (det == 0.0f) ? 0.0f : 1.0f / det;

            glm::vec3 tangent;
            if (f != 0.0f) {
                tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

                tempTangents[i1] += tangent; tempTangents[i2] += tangent; tempTangents[i3] += tangent;
            }
        }

        std::vector<float> newVertices;
        newVertices.reserve(numVertices * OUTPUT_STRIDE_LOCAL);

        for (size_t i = 0; i < numVertices; ++i) {
            glm::vec3 N = glm::make_vec3(&vertices[i * INPUT_STRIDE_LOCAL + 3]);
            glm::vec3 T = glm::normalize(tempTangents[i]);

            // Gram-Schmidt ortogonalizace (T je kolmé k N)
            T = glm::normalize(T - glm::dot(T, N) * N);

            // Zkopírování P(3), N(3), UV(2)
            for (int j = 0; j < INPUT_STRIDE_LOCAL; ++j) {
                newVertices.push_back(vertices[i * INPUT_STRIDE_LOCAL + j]);
            }
            // Přidání T(3)
            newVertices.push_back(T.x); newVertices.push_back(T.y); newVertices.push_back(T.z);
        }

        vertices = std::move(newVertices); //  (stride 8) TO(stride 11)
    }

    // =========================================================================================
    // data Stride 8 (P, N, UV) INIT mesh
    // =========================================================================================
    StaticMesh(const std::vector<float>& initialVertices,
               const std::vector<unsigned int>& initialIndices,
               PbrMaterial* mat)
        : material(mat)
    {

        UpdateGeometry(initialVertices, initialIndices);
    }


    ~StaticMesh() {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
    }

    void Draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& cameraPos, unsigned int envCubemap, unsigned int shadowMap,
              const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDir, const glm::vec3& lightCol) const
    {
        if (!material || VAO == 0) return;

        material->use(model, view, proj, cameraPos, envCubemap, shadowMap, lightSpaceMatrix, lightDir, lightCol);
        if(material->transmission > 0.0){
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        if(material->transmission > 0.0){
            glDisable(GL_BLEND);
        }
        material->unuse();
    }

    void DrawForShadow(unsigned int depthShader, const glm::mat4& model, const glm::mat4& lightSpaceMatrix) const {
        if (VAO == 0 || indexCount == 0) return;

        glUseProgram(depthShader);
        glUniformMatrix4fv(glGetUniformLocation(depthShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(depthShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // =========================================================================================
    // data STRIDE 8,  tangentS TO  STRIDE 11
    // =========================================================================================
    void UpdateGeometry(const std::vector<float>& inputVertices, const std::vector<unsigned int>& inputIndices)
    {

        if (inputVertices.size() % INPUT_STRIDE != 0) {
            std::cerr << "CHYBA: UpdateGeometry: Vstupní data (P, N, UV) nemají očekávaný Stride " << INPUT_STRIDE << "." << std::endl;
            indexCount = 0;
            return;
        }

        // COPY (Stride 8) a transforma (Stride 11)
        this->vertices = inputVertices;
        this->indices = inputIndices;

        CalculateTangents(this->vertices, this->indices); //  Stride 8 -> Stride 11

        if (this->vertices.size() % VERTEX_STRIDE != 0) {
            std::cerr << "CHYBA: UpdateGeometry: Chyba po CalculateTangents! Výstupní Stride NENÍ " << VERTEX_STRIDE << "." << std::endl;
            indexCount = 0;
            return;
        }

        indexCount = static_cast<unsigned int>(this->indices.size());

        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        // VBO (Vertices -  Stride 11)
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(float), this->vertices.data(), GL_DYNAMIC_DRAW);

        // EBO (Indices)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(unsigned int), this->indices.data(), GL_DYNAMIC_DRAW);

        // 3.  atributS (Stride 11)
        GLsizei stride = VERTEX_STRIDE * sizeof(float);

        // Pozice (P) - Location 0, Offset 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

        // Norm(N) - Location 1, Offset 3
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

        // UV - Location 2, Offset 6
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

        // Tangenta (T) - Location 3, Offset 8
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));

        glBindVertexArray(0);
    }
};

#endif // STATICMESH_H
