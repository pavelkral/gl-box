#ifndef BUFFER_H
#define BUFFER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


class Buffer {
public:
    GLuint ID = 0;
    GLenum type;

    Buffer(GLenum type);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;

    void bind() const;
    void unbind() const;
     void allocate(size_t size, GLenum usage = GL_DYNAMIC_DRAW) ;

    template<typename T>
    void setData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
        bind();
        glBufferData(type, data.size() * sizeof(T), data.data(), usage);
    }


    template<typename T>
    void setSubData(const std::vector<T>& data, size_t offset = 0) {
        bind();
        glBufferSubData(type, offset, data.size() * sizeof(T), data.data());
    }

    template<typename T>
    void setSubDataSingle(const T& data, size_t offset = 0) {
        bind();
        glBufferSubData(type, offset, sizeof(T), &data);
    }
};

#endif // BUFFER_H
