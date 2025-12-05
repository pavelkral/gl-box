#include "vertexarray.h"

VertexArray::VertexArray() { glGenVertexArrays(1, &ID); }

VertexArray::~VertexArray() { if(ID) glDeleteVertexArrays(1, &ID); }

VertexArray::VertexArray(VertexArray &&other) noexcept : ID(other.ID) { other.ID = 0; }

void VertexArray::bind() const { glBindVertexArray(ID); }

void VertexArray::unbind() const { glBindVertexArray(0); }
