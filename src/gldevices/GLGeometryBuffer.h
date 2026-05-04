#pragma once

#include "abstract/GeometryBuffer.h"
#include "gldevices/GLIndexBuffer.h"
#include "gldevices/GLVertexBuffer.h"

namespace gldevices {

// OpenGL geometry buffer owning a GLVertexBuffer (VBO + VAO) and a GLIndexBuffer (IBO).
//
// The IBO is captured in the VAO during construction, so Bind() restores vertex
// attributes and the index buffer binding in a single VAO bind call.
class GLGeometryBuffer : public abstract::GeometryBuffer {
 public:
  GLGeometryBuffer(core::VertexType vertex_type, int num_vertices,
                   abstract::IndexType index_type, int num_indices,
                   abstract::BufferUsage usage);
  ~GLGeometryBuffer() override = default;

  // Binds the VAO, which restores vertex attributes and the captured IBO binding.
  void Bind() override;

  // Uploads num_vertices vertices from data into the vertex buffer at the given offset.
  void FillVertices(const void* data, int num_vertices, int offset = 0) override;

  // Uploads num_indices indices from data into the index buffer at the given offset.
  void FillIndices(const void* data, int num_indices, int offset = 0) override;

 private:
  GLVertexBuffer vb_;
  GLIndexBuffer  ib_;
};

}  // namespace gldevices
