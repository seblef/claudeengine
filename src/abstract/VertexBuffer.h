#pragma once

#include "abstract/BufferUsage.h"
#include "core/VertexType.h"

namespace abstract {

// Abstract vertex buffer. Holds a typed array of vertices on the GPU.
//
// Concrete implementations allocate GPU memory in their constructor and
// implement Bind() to set this buffer as the active source for draw calls,
// and Fill() to upload vertex data (or a sub-range of it).
class VertexBuffer {
 public:
  VertexBuffer(core::VertexType vertex_type, int num_vertices, BufferUsage usage)
      : vertex_type_(vertex_type), num_vertices_(num_vertices), usage_(usage) {}

  virtual ~VertexBuffer() = default;

  // Sets this buffer as the active vertex source for subsequent draw calls.
  virtual void Bind() = 0;

  // Uploads vertex data into the buffer.
  // data         — pointer to the source vertex array
  // num_vertices — number of vertices to copy
  // offset       — number of vertices to skip at the start of the buffer (default 0)
  virtual void Fill(const void* data, int num_vertices, int offset = 0) = 0;

  [[nodiscard]] core::VertexType GetVertexType() const { return vertex_type_; }
  [[nodiscard]] int              GetNumVertices() const { return num_vertices_; }
  [[nodiscard]] BufferUsage      GetBufferUsage() const { return usage_; }

 protected:
  core::VertexType vertex_type_;
  int              num_vertices_;
  BufferUsage      usage_;
};

}  // namespace abstract
