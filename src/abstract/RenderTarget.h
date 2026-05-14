#pragma once

#include "abstract/TextureFormat.h"

namespace abstract {

// Off-screen GPU texture usable as a color or depth+stencil attachment.
//
// Lifecycle: created via VideoDevice::CreateRenderTarget(); owned exclusively
// by the caller via unique_ptr.  The RenderTargetGroup that references this
// target must be destroyed before the target itself.
class RenderTarget {
 public:
  RenderTarget(int width, int height, TextureFormat format)
      : width_(width), height_(height), format_(format) {}

  virtual ~RenderTarget() = default;

  RenderTarget(const RenderTarget&)            = delete;
  RenderTarget& operator=(const RenderTarget&) = delete;

  // Binds the texture as a sampler at the given texture unit slot.
  virtual void BindAsSampler(int slot) = 0;

  // Enables or disables depth-comparison mode (no-op for non-depth formats).
  // Call with false before binding a depth RT as a plain sampler2D for
  // visualization; restore with true afterwards.
  virtual void EnableCompareMode(bool /*enable*/) {}

  // Attaches the texture to the currently bound FBO.
  // For color formats: attaches at GL_COLOR_ATTACHMENT0 + index.
  // For depth+stencil formats: attaches at GL_DEPTH_STENCIL_ATTACHMENT (index is ignored).
  virtual void BindAsOutput(int index) = 0;

  // Returns the backend-specific texture handle as a void*.
  // On OpenGL: (void*)(intptr_t)gl_texture_id — suitable for ImGui::Image().
  [[nodiscard]] virtual void* GetNativeHandle() const = 0;

  [[nodiscard]] int           GetWidth()  const { return width_; }
  [[nodiscard]] int           GetHeight() const { return height_; }
  [[nodiscard]] TextureFormat GetFormat() const { return format_; }

 protected:
  int           width_;
  int           height_;
  TextureFormat format_;
};

}  // namespace abstract
