#ifndef MESH_H
#define MESH_H

#include "../gl/buffer.h"
#include "../gl/vertexarray.h"
#include <memory>

 struct Mesh {
    std::unique_ptr<VertexArray> vao;
    std::unique_ptr<Buffer> vbo;
    std::unique_ptr<Buffer> ebo;
    GLsizei indexCount = 0;

    Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);

    void drawInstanced(GLsizei count) const;
};

#endif // MESH_H
