#pragma once

#include "abstract/BufferUsage.h"
#include "abstract/IndexType.h"
#include "core/VertexType.h"

namespace abstract {

// Abstract geometry buffer: combines a vertex buffer and an index buffer into
// a single managed object, simplifying draw-call setup.
//
// Concrete implementations allocate GPU resources in their constructor,
// implement Bind() to activate both buffers for indexed draw calls, and
// provide FillVertices() / FillIndices() for data upload.
class GeometryBuffer {
 public:
  GeometryBuffer(core::VertexType vertex_type, int num_vertices,
                 IndexType index_type, int num_indices,
                 BufferUsage usage)
      : vertex_type_(vertex_type), num_vertices_(num_vertices),
        index_type_(index_type), num_indices_(num_indices),
        usage_(usage) {}

  virtual ~GeometryBuffer() = default;

  // Activates both the vertex and index buffers for subsequent indexed draw calls.
  virtual void Bind() = 0;

  // Uploads vertex data into the vertex buffer.
  // num_vertices — number of vertices to copy
  // offset       — vertex offset into the buffer (default 0)
  virtual void FillVertices(const void* data, int num_vertices, int offset = 0) = 0;

  // Uploads index data into the index buffer.
  // num_indices — number of indices to copy
  // offset      — index offset into the buffer (default 0)
  virtual void FillIndices(const void* data, int num_indices, int offset = 0) = 0;

  [[nodiscard]] core::VertexType GetVertexType()  const { return vertex_type_; }
  [[nodiscard]] int              GetNumVertices()  const { return num_vertices_; }
  [[nodiscard]] IndexType        GetIndexType()    const { return index_type_; }
  [[nodiscard]] int              GetNumIndices()   const { return num_indices_; }
  [[nodiscard]] BufferUsage      GetBufferUsage()  const { return usage_; }

 protected:
  core::VertexType vertex_type_;
  int              num_vertices_;
  IndexType        index_type_;
  int              num_indices_;
  BufferUsage      usage_;
};

}  // namespace abstract
