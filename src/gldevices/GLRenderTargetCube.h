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

  // Binds the cube texture to GL_TEXTURE0 + slot for sampling.
  void BindAsSampler(int slot) override;

  // Attaches the given cube face to the currently bound FBO as depth attachment.
  void BindFaceAsOutput(int face_idx) override;

 private:
  GLuint tex_id_ = 0;
};

}  // namespace gldevices
