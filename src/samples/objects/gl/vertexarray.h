#ifndef VERTEXARRAY_H
#define VERTEXARRAY_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class VertexArray {
public:
    GLuint ID = 0;
    VertexArray();
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray(VertexArray&& other) noexcept;

    void bind() const;
    void unbind() const;
};
#endif // VERTEXARRAY_H
