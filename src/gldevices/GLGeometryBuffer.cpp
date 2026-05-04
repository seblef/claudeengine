#include "gldevices/GLGeometryBuffer.h"

namespace gldevices {

GLGeometryBuffer::GLGeometryBuffer(core::VertexType vertex_type, int num_vertices,
                                   abstract::IndexType index_type, int num_indices,
                                   abstract::BufferUsage usage)
    : abstract::GeometryBuffer(vertex_type, num_vertices, index_type, num_indices, usage),
      vb_(vertex_type, num_vertices, usage),
      ib_(index_type, num_indices, usage) {
  // Bind the VAO created by vb_, then bind the IBO so it is captured in the VAO.
  // Subsequent Bind() calls only need to bind the VAO to restore both.
  vb_.Bind();
  ib_.Bind();
}

void GLGeometryBuffer::Bind() {
  vb_.Bind();
}

void GLGeometryBuffer::FillVertices(const void* data, int num_vertices, int offset) {
  vb_.Fill(data, num_vertices, offset);
}

void GLGeometryBuffer::FillIndices(const void* data, int num_indices, int offset) {
  ib_.Fill(data, num_indices, offset);
}

}  // namespace gldevices
