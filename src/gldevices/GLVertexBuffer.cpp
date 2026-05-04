// Must be defined before any GL/gl.h include (which pulls in glext.h internally).
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLVertexBuffer.h"

#include "core/VertexType.h"

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

GLVertexBuffer::GLVertexBuffer(core::VertexType vertex_type, int num_vertices,
                               abstract::BufferUsage usage)
    : abstract::VertexBuffer(vertex_type, num_vertices, usage) {
  const GLsizeiptr byte_size =
      static_cast<GLsizeiptr>(core::kVertexSize[static_cast<size_t>(vertex_type)] * num_vertices);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, byte_size, nullptr, ToGLUsage(usage));
}

GLVertexBuffer::~GLVertexBuffer() {
  glDeleteBuffers(1, &vbo_);
}

void GLVertexBuffer::Bind() {
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
}

void GLVertexBuffer::Fill(const void* data, int num_vertices, int offset) {
  const size_t vertex_size = core::kVertexSize[static_cast<size_t>(vertex_type_)];
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER,
                  static_cast<GLintptr>(offset * vertex_size),
                  static_cast<GLsizeiptr>(num_vertices * vertex_size),
                  data);
}

}  // namespace gldevices
