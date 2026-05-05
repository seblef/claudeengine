#pragma once

#include "abstract/BufferUsage.h"

namespace abstract {

// Abstract constant (DirectX) / uniform (OpenGL) buffer.
// Stores a fixed-size block of shader-visible data at a given slot/binding point.
// size is expressed in float4 units; the byte size is size * 4 * sizeof(float).
class ConstantBuffer {
 public:
  ConstantBuffer(int size, int slot, BufferUsage usage)
      : size_(size), slot_(slot), usage_(usage) {}
  virtual ~ConstantBuffer() = default;

  // Binds this buffer to its slot for subsequent draw calls.
  virtual void Bind() = 0;

  // Uploads the full buffer content from data (size_ float4s = size_*16 bytes).
  virtual void Fill(const void* data) = 0;

  [[nodiscard]] int         GetSize()        const { return size_; }
  [[nodiscard]] int         GetSlot()        const { return slot_; }
  [[nodiscard]] BufferUsage GetBufferUsage() const { return usage_; }

 protected:
  int         size_;
  int         slot_;
  BufferUsage usage_;
};

}  // namespace abstract
