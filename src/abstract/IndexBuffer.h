#pragma once

#include "abstract/BufferUsage.h"
#include "abstract/IndexType.h"

namespace abstract {

// Abstract index buffer. Holds a typed array of vertex indices on the GPU.
//
// Concrete implementations allocate GPU memory in their constructor and
// implement Bind() to set this buffer as the active index source for draw
// calls, and Fill() to upload index data (or a sub-range of it).
class IndexBuffer {
 public:
  IndexBuffer(IndexType index_type, int num_indices, BufferUsage usage)
      : index_type_(index_type), num_indices_(num_indices), usage_(usage) {}

  virtual ~IndexBuffer() = default;

  // Sets this buffer as the active index source for subsequent draw calls.
  virtual void Bind() = 0;

  // Uploads index data into the buffer.
  // data        — pointer to the source index array
  // num_indices — number of indices to copy
  // offset      — number of indices to skip at the start of the buffer (default 0)
  virtual void Fill(const void* data, int num_indices, int offset = 0) = 0;

  [[nodiscard]] IndexType   GetIndexType()   const { return index_type_; }
  [[nodiscard]] int         GetNumIndices()  const { return num_indices_; }
  [[nodiscard]] BufferUsage GetBufferUsage() const { return usage_; }

 protected:
  IndexType   index_type_;
  int         num_indices_;
  BufferUsage usage_;
};

}  // namespace abstract
