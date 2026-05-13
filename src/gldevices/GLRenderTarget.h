#pragma once

#include <GL/gl.h>

#include "abstract/RenderTarget.h"

namespace gldevices {

// OpenGL render target: a GL_TEXTURE_2D allocated for off-screen rendering.
//
// Supports color formats (RGBA8, RGBA16F, G11R11B10F), the combined
// depth+stencil format (Depth24Stencil8), and the depth-only format
// (Depth32F) used for shadow maps.  Created via
// GLVideoDevice::CreateRenderTarget().
//
// Depth32F targets are configured at construction for hardware PCF:
// LINEAR filter and GL_COMPARE_REF_TO_TEXTURE compare mode, enabling use
// as sampler2DShadow in GLSL.
class GLRenderTarget : public abstract::RenderTarget {
 public:
  GLRenderTarget(int width, int height, abstract::TextureFormat format);
  ~GLRenderTarget() override;

  // Binds the texture to GL_TEXTURE0 + slot for sampling in shaders.
  void BindAsSampler(int slot) override;

  // Attaches the texture to the currently bound FBO.
  // Color formats: attaches at GL_COLOR_ATTACHMENT0 + index.
  // Depth+stencil format: attaches at GL_DEPTH_STENCIL_ATTACHMENT (index ignored).
  // Depth32F format: attaches at GL_DEPTH_ATTACHMENT (index ignored).
  void BindAsOutput(int index) override;

 private:
  GLuint tex_id_ = 0;
};

}  // namespace gldevices
