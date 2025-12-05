#include "mesh.h"

Mesh::Mesh(const std::vector<float> &vertices, const std::vector<unsigned int> &indices) {
    vao = std::make_unique<VertexArray>();
    vbo = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    ebo = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    vao->bind();
    vbo->setData(vertices);
    ebo->setData(indices);

    // Layout 0: Pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    vao->unbind();
    indexCount = (GLsizei)indices.size();
}

void Mesh::drawInstanced(GLsizei count) const {
    vao->bind();
    glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, count);
    vao->unbind();
}
