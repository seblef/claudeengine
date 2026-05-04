#pragma once

#include <memory>
#include <string>

#include "abstract/BufferUsage.h"
#include "abstract/GeometryBuffer.h"
#include "abstract/IndexBuffer.h"
#include "abstract/IndexType.h"
#include "abstract/PrimitiveType.h"
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

  // ---- Render state --------------------------------------------------------

  // Sets the primitive topology for subsequent Render() and RenderIndexed() calls.
  virtual void SetPrimitiveType(PrimitiveType type) = 0;

  // Sets the index element type for subsequent RenderIndexed() calls.
  virtual void SetIndexType(IndexType type) = 0;

  // Draws num_vertices vertices from the currently bound vertex buffer,
  // starting at first_vertex.
  virtual void Render(int num_vertices, int first_vertex = 0) = 0;

  // Draws num_indices indexed primitives from the currently bound index and
  // vertex buffers.
  // first_index   — first index in the index buffer to use (default 0)
  // vertex_offset — constant added to each index before fetching the vertex (default 0)
  virtual void RenderIndexed(int num_indices, int first_index = 0,
                              int vertex_offset = 0) = 0;

  // ---- Resource factories --------------------------------------------------

  // Creates a vertex buffer with the given layout, capacity and access hint.
  // If data is non-null the buffer is filled immediately starting at offset.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<VertexBuffer> CreateVertexBuffer(
      core::VertexType vertex_type, int num_vertices, BufferUsage usage,
      const void* data = nullptr, int offset = 0) = 0;

  // Creates an index buffer with the given type, capacity and access hint.
  // If data is non-null the buffer is filled immediately starting at offset.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<IndexBuffer> CreateIndexBuffer(
      IndexType type, int num_indices, BufferUsage usage,
      const void* data = nullptr, int offset = 0) = 0;

  // Creates a geometry buffer combining vertex and index data with a shared usage hint.
  // If vertex_data / index_data are non-null the buffers are filled immediately.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<GeometryBuffer> CreateGeometryBuffer(
      core::VertexType vertex_type, int num_vertices,
      IndexType index_type, int num_indices,
      BufferUsage usage,
      const void* vertex_data = nullptr, int vertex_offset = 0,
      const void* index_data = nullptr, int index_offset = 0) = 0;

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
