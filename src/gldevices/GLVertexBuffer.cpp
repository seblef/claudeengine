// Must be defined before any GL/gl.h include (which pulls in glext.h internally).
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLVertexBuffer.h"

#include "core/Vertex2D.h"
#include "core/Vertex3D.h"
#include "core/VertexBase.h"
#include "core/VertexParticle.h"
#include "core/VertexTerrain.h"
#include "core/VertexType.h"

#include <cstddef>

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

// Configures vertex attribute pointers for the given layout.
// Must be called with the target VAO and VBO already bound.
void ConfigureAttributes(core::VertexType vertex_type) {
  switch (vertex_type) {
    case core::VertexType::kBase:
    case core::VertexType::k2D: {
      const GLsizei stride = static_cast<GLsizei>(sizeof(core::VertexBase));
      // location 0: position (vec3)
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::VertexBase, position)));
      // location 1: color (vec4)
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::VertexBase, color)));
      // location 2: uv (vec2)
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::VertexBase, uv)));
      break;
    }
    case core::VertexType::k3D: {
      const GLsizei stride = static_cast<GLsizei>(sizeof(core::Vertex3D));
      // location 0: position (vec3)
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::Vertex3D, position)));
      // location 1: normal (vec3)
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::Vertex3D, normal)));
      // location 2: binormal (vec3)
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::Vertex3D, binormal)));
      // location 3: tangent (vec3)
      glEnableVertexAttribArray(3);
      glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::Vertex3D, tangent)));
      // location 4: uv (vec2)
      glEnableVertexAttribArray(4);
      glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::Vertex3D, uv)));
      break;
    }
    case core::VertexType::kTerrain: {
      const GLsizei stride = static_cast<GLsizei>(sizeof(core::VertexTerrain));
      // location 0: xz position (vec2)
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::VertexTerrain, xz)));
      break;
    }
    case core::VertexType::kParticle: {
      const GLsizei stride = static_cast<GLsizei>(sizeof(core::VertexParticle));
      // location 0: center (vec3)
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::VertexParticle, center)));
      // location 1: size (float)
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::VertexParticle, size)));
      // location 2: color (vec4)
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::VertexParticle, color)));
      // location 3: uv_offset (vec2)
      glEnableVertexAttribArray(3);
      glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<const void*>(offsetof(core::VertexParticle, uv_offset)));
      break;
    }
    default:
      break;
  }
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

  // Build a VAO that captures the VBO binding and attribute layout so that
  // Bind() restores all state in one call, and glDrawArrays works in core profile.
  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  ConfigureAttributes(vertex_type);
  glBindVertexArray(0);
}

GLVertexBuffer::~GLVertexBuffer() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &vbo_);
}

void GLVertexBuffer::Bind() {
  glBindVertexArray(vao_);
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
