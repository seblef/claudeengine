// Must be defined before any GL/gl.h include to expose extension prototypes.
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLConstantBuffer.h"

namespace {

GLenum ToGLUsage(abstract::BufferUsage usage) {
  switch (usage) {
    case abstract::BufferUsage::kDynamic:   return GL_DYNAMIC_DRAW;
    case abstract::BufferUsage::kImmutable: return GL_STATIC_DRAW;
    case abstract::BufferUsage::kStaging:   return GL_STREAM_DRAW;
  }
  return GL_DYNAMIC_DRAW;
}

}  // namespace

namespace gldevices {

GLConstantBuffer::GLConstantBuffer(int size, int slot, abstract::BufferUsage usage)
    : abstract::ConstantBuffer(size, slot, usage) {
  const GLsizeiptr byte_size = static_cast<GLsizeiptr>(size * 4 * sizeof(float));
  glGenBuffers(1, &ubo_);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
  glBufferData(GL_UNIFORM_BUFFER, byte_size, nullptr, ToGLUsage(usage));
}

GLConstantBuffer::~GLConstantBuffer() {
  glDeleteBuffers(1, &ubo_);
}

void GLConstantBuffer::Bind() {
  glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(slot_), ubo_);
}

void GLConstantBuffer::Fill(const void* data) {
  const GLsizeiptr byte_size = static_cast<GLsizeiptr>(size_ * 4 * sizeof(float));
  glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, byte_size, data);
}

}  // namespace gldevices
