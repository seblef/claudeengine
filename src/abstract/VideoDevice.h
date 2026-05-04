#pragma once

#include <memory>
#include <string>

#include "abstract/BufferUsage.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "core/Color.h"
#include "core/VertexType.h"

namespace abstract {

// Abstract GPU backend. Concrete subclasses implement OpenGL and DirectX.
//
// Constructed by the platform Devices class, which passes the initial
// resolution and display mode. All factory methods return heap-allocated
// objects; see individual method docs for ownership semantics.
class VideoDevice {
 public:
  VideoDevice(int width, int height, bool windowed)
      : width_(width), height_(height), windowed_(windowed) {}

  virtual ~VideoDevice() = default;

  // ---- Frame lifecycle -----------------------------------------------------

  // Called at the start of each frame to prepare the backend for rendering.
  virtual void BeginFrame() = 0;

  // Clears all active render targets to the given background color.
  virtual void ClearRenderTargets(const core::Color& color) = 0;

  // ---- Window management ---------------------------------------------------

  // Called when the window or screen resolution changes.
  virtual void OnResize(int width, int height) = 0;

  // ---- Resource factories --------------------------------------------------

  // Creates a vertex buffer with the given layout, capacity and access hint.
  // If data is non-null the buffer is filled immediately starting at offset.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<VertexBuffer> CreateVertexBuffer(
      core::VertexType vertex_type, int num_vertices, BufferUsage usage,
      const void* data = nullptr, int offset = 0) = 0;

  // Creates (or retrieves) a shader by name. The resource registry starts
  // the object with ref_count = 1; call Release() when done.
  [[nodiscard]] virtual Shader* CreateShader(const std::string& name) = 0;

  // ---- Getters -------------------------------------------------------------

  [[nodiscard]] int  GetWidth()   const { return width_; }
  [[nodiscard]] int  GetHeight()  const { return height_; }
  [[nodiscard]] bool IsWindowed() const { return windowed_; }

 protected:
  int  width_;
  int  height_;
  bool windowed_;
};

}  // namespace abstract
