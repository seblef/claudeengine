#pragma once

#include <GL/gl.h>

#include "abstract/ConstantBuffer.h"

namespace gldevices {

// OpenGL uniform buffer object (UBO) implementation of ConstantBuffer.
// Bind() calls glBindBufferBase to associate the UBO with its binding slot.
// Fill() uploads the full buffer content via glBufferSubData.
class GLConstantBuffer : public abstract::ConstantBuffer {
 public:
  GLConstantBuffer(int size, int slot, abstract::BufferUsage usage);
  ~GLConstantBuffer() override;

  void Bind() override;
  void Fill(const void* data) override;

 private:
  GLuint ubo_ = 0;
};

}  // namespace gldevices
