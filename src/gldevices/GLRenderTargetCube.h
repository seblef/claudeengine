#pragma once

#include <GL/gl.h>

#include "abstract/RenderTargetCube.h"

namespace gldevices {

// OpenGL cube-map depth render target for omni-light shadow rendering.
//
// Allocates a GL_TEXTURE_CUBE_MAP with GL_DEPTH_COMPONENT32F at construction.
// Configured for hardware PCF: LINEAR filter and GL_COMPARE_REF_TO_TEXTURE
// compare mode, enabling use as samplerCubeShadow in GLSL.
//
// BindFaceAsOutput() calls glFramebufferTexture2D on the caller's currently
// bound FBO — the caller must bind its FBO before calling this method.
class GLRenderTargetCube : public abstract::RenderTargetCube {
 public:
  explicit GLRenderTargetCube(int size);
  ~GLRenderTargetCube() override;

  void BindAsSampler(int slot) override;
  void BindFaceAsOutput(int face_idx) override;
  void EnableCompareMode(bool enable) override;

 private:
  GLuint tex_id_ = 0;
};

}  // namespace gldevices
