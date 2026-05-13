#pragma once

namespace abstract {

// Off-screen GPU cube-map texture usable as a depth-only cube map shadow target.
//
// Lifecycle: created via VideoDevice::CreateRenderTargetCube(); owned
// exclusively by the caller via unique_ptr.
//
// The calling code is responsible for binding an FBO before calling
// BindFaceAsOutput(), then unbinding it when done.
class RenderTargetCube {
 public:
  virtual ~RenderTargetCube() = default;

  RenderTargetCube(const RenderTargetCube&)            = delete;
  RenderTargetCube& operator=(const RenderTargetCube&) = delete;

  // Binds the cube map as a samplerCubeShadow at the given texture unit.
  virtual void BindAsSampler(int slot) = 0;

  // Attaches face `face_idx` (0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z) to the
  // currently bound FBO as GL_DEPTH_ATTACHMENT.
  virtual void BindFaceAsOutput(int face_idx) = 0;

  [[nodiscard]] int GetSize() const { return size_; }

 protected:
  explicit RenderTargetCube(int size) : size_(size) {}
  int size_;
};

}  // namespace abstract
