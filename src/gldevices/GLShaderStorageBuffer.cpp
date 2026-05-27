#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLShaderStorageBuffer.h"

#include "abstract/BufferUsage.h"

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

}  // namespace

GLShaderStorageBuffer::GLShaderStorageBuffer(int capacity_bytes,
                                             int binding_point,
                                             abstract::BufferUsage usage)
    : abstract::ShaderStorageBuffer(capacity_bytes, binding_point, usage) {
  glGenBuffers(1, &ssbo_);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               static_cast<GLsizeiptr>(capacity_bytes),
               nullptr, ToGLUsage(usage));
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

GLShaderStorageBuffer::~GLShaderStorageBuffer() {
  glDeleteBuffers(1, &ssbo_);
}

void GLShaderStorageBuffer::Bind() {
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                   static_cast<GLuint>(binding_point_), ssbo_);
}

void GLShaderStorageBuffer::Fill(const void* data, int size_bytes) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                  static_cast<GLsizeiptr>(size_bytes), data);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

}  // namespace gldevices
