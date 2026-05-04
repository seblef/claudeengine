#pragma once

#include "abstract/IndexBuffer.h"

#include <GL/gl.h>

namespace gldevices {

// OpenGL index buffer backed by a IBO (GL_ELEMENT_ARRAY_BUFFER).
//
// Allocates an IBO on construction and destroys it on destruction.
// Bind() should be called while a vertex buffer's VAO is active so that
// the IBO binding is captured in the VAO state.
class GLIndexBuffer : public abstract::IndexBuffer {
 public:
  GLIndexBuffer(abstract::IndexType index_type, int num_indices,
                abstract::BufferUsage usage);
  ~GLIndexBuffer() override;

  // Binds this IBO to GL_ELEMENT_ARRAY_BUFFER.
  void Bind() override;

  // Uploads num_indices indices from data into the buffer at the given index offset.
  void Fill(const void* data, int num_indices, int offset = 0) override;

 private:
  GLuint ibo_ = 0;
};

}  // namespace gldevices
