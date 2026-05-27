#pragma once

#include "abstract/BufferUsage.h"

namespace abstract {

// Abstract Shader Storage Buffer Object (SSBO).
//
// Stores an arbitrary byte array on the GPU, accessible from shaders at a
// fixed binding point declared with layout(std430, binding = N).
//
// The capacity is fixed at construction time.  Fill() uploads a sub-range;
// the full buffer contents past the uploaded range are undefined until filled.
class ShaderStorageBuffer {
 public:
  ShaderStorageBuffer(int capacity_bytes, int binding_point, BufferUsage usage)
      : capacity_bytes_(capacity_bytes),
        binding_point_(binding_point),
        usage_(usage) {}

  virtual ~ShaderStorageBuffer() = default;

  // Binds this SSBO to its binding point for subsequent draw calls.
  virtual void Bind() = 0;

  // Uploads size_bytes of data from the CPU pointer into the start of the SSBO.
  // size_bytes must be <= capacity_bytes_.
  virtual void Fill(const void* data, int size_bytes) = 0;

  [[nodiscard]] int         GetCapacityBytes() const { return capacity_bytes_; }
  [[nodiscard]] int         GetBindingPoint()  const { return binding_point_;  }
  [[nodiscard]] BufferUsage GetUsage()         const { return usage_;          }

 protected:
  int         capacity_bytes_;
  int         binding_point_;
  BufferUsage usage_;
};

}  // namespace abstract
