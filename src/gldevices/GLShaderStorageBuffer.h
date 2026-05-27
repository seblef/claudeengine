#pragma once

#include <GL/gl.h>

#include "abstract/ShaderStorageBuffer.h"

namespace gldevices {

// OpenGL Shader Storage Buffer Object (GL_SHADER_STORAGE_BUFFER).
//
// Allocates a VBO as an SSBO on construction and destroys it on destruction.
// Fill() uses glBufferSubData; Bind() calls glBindBufferBase.
class GLShaderStorageBuffer : public abstract::ShaderStorageBuffer {
 public:
  GLShaderStorageBuffer(int capacity_bytes, int binding_point,
                        abstract::BufferUsage usage);
  ~GLShaderStorageBuffer() override;

  // Binds the SSBO to GL_SHADER_STORAGE_BUFFER at the stored binding point.
  void Bind() override;

  // Uploads size_bytes bytes from data into the start of the SSBO.
  void Fill(const void* data, int size_bytes) override;

 private:
  GLuint ssbo_ = 0;
};

}  // namespace gldevices
