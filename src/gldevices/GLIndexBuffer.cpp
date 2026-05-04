// Must be defined before any GL/gl.h include (which pulls in glext.h internally).
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLIndexBuffer.h"

namespace gldevices {

namespace {

GLenum ToGLUsage(abstract::BufferUsage usage) {
  switch (usage) {
    case abstract::BufferUsage::kDynamic:   return GL_DYNAMIC_DRAW;
    case abstract::BufferUsage::kImmutable: return GL_STATIC_DRAW;
    case abstract::BufferUsage::kStaging:   return GL_STREAM_DRAW;
  }
  return GL_DYNAMIC_DRAW;
}

int IndexByteSize(abstract::IndexType type) {
  return (type == abstract::IndexType::kUInt16) ? 2 : 4;
}

}  // namespace

GLIndexBuffer::GLIndexBuffer(abstract::IndexType index_type, int num_indices,
                             abstract::BufferUsage usage)
    : abstract::IndexBuffer(index_type, num_indices, usage) {
  const GLsizeiptr byte_size =
      static_cast<GLsizeiptr>(IndexByteSize(index_type) * num_indices);
  glGenBuffers(1, &ibo_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, byte_size, nullptr, ToGLUsage(usage));
}

GLIndexBuffer::~GLIndexBuffer() {
  glDeleteBuffers(1, &ibo_);
}

void GLIndexBuffer::Bind() {
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
}

void GLIndexBuffer::Fill(const void* data, int num_indices, int offset) {
  const int index_size = IndexByteSize(index_type_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                  static_cast<GLintptr>(offset * index_size),
                  static_cast<GLsizeiptr>(num_indices * index_size),
                  data);
}

}  // namespace gldevices
