#include "buffer.h"

Buffer::Buffer(GLenum type) : type(type) { glGenBuffers(1, &ID); }

Buffer::~Buffer() { if(ID) glDeleteBuffers(1, &ID); }

Buffer::Buffer(Buffer &&other) noexcept : ID(other.ID), type(other.type) { other.ID = 0; }

void Buffer::bind() const { glBindBuffer(type, ID); }

void Buffer::unbind() const { glBindBuffer(type, 0); }

void Buffer::allocate(size_t size, GLenum usage) {
    bind();
    glBufferData(type, size, nullptr, usage);
}
