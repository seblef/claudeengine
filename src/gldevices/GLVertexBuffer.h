#pragma once

#include "abstract/BufferUsage.h"
#include "abstract/VertexBuffer.h"
#include "core/VertexType.h"

#include <GL/gl.h>

namespace gldevices {

// OpenGL vertex buffer backed by a VBO.
//
// Allocates a VBO on construction and destroys it on destruction.
// Fill() uses glBufferSubData and requires an active GL context.
class GLVertexBuffer : public abstract::VertexBuffer {
 public:
  GLVertexBuffer(core::VertexType vertex_type, int num_vertices,
                 abstract::BufferUsage usage);
  ~GLVertexBuffer() override;

  // Binds the VBO to GL_ARRAY_BUFFER.
  void Bind() override;

  // Uploads num_vertices vertices from data into the buffer at the given vertex offset.
  void Fill(const void* data, int num_vertices, int offset = 0) override;

 private:
  GLuint vbo_ = 0;
};

}  // namespace gldevices
