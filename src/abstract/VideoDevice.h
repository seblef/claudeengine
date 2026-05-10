#pragma once

#include <memory>
#include <span>
#include <string>

#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/ConstantBuffer.h"
#include "abstract/CullFace.h"
#include "abstract/Face.h"
#include "abstract/GeometryBuffer.h"
#include "abstract/IndexBuffer.h"
#include "abstract/IndexType.h"
#include "abstract/PrimitiveType.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/StencilOp.h"
#include "abstract/Texture.h"
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

  // Creates a constant buffer of size float4 elements bound to the given slot.
  // If data is non-null the buffer is filled immediately.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<ConstantBuffer> CreateConstantBuffer(
      int size, int slot, BufferUsage usage = BufferUsage::kDynamic,
      const void* data = nullptr) = 0;

  // Enables or disables depth testing for subsequent draw calls.
  virtual void SetDepthTestEnabled(bool enabled) = 0;

  // Enables or disables writing to the color buffer.
  // Disable before the stencil-fill sub-pass of a light volume.
  virtual void SetColorWriteEnabled(bool enabled) = 0;

  // Sets the face culling mode for subsequent draw calls.
  virtual void SetFaceCulling(CullFace mode) = 0;

  // Enables or disables the stencil test for subsequent draw calls.
  virtual void SetStencilTestEnabled(bool enabled) = 0;

  // Sets the stencil comparison function.
  // ref and mask are applied as: (stencil_buffer & mask) <func> (ref & mask).
  virtual void SetStencilFunc(CompareFunc func, int ref, unsigned mask) = 0;

  // Sets the stencil operation applied to the given polygon face when:
  //   sfail  — stencil test fails.
  //   dpfail — stencil test passes but depth test fails.
  //   dppass — both tests pass.
  virtual void SetStencilOp(Face face, StencilOp sfail,
                             StencilOp dpfail, StencilOp dppass) = 0;

  // Clears the stencil buffer to val.
  virtual void ClearStencil(int val) = 0;

  // Creates an off-screen render target texture of the given format.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<RenderTarget> CreateRenderTarget(
      int width, int height, TextureFormat format) = 0;

  // Creates an FBO bundle from color_targets + one depth+stencil target.
  // All targets must have been created by this device and must outlive the group.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<RenderTargetGroup> CreateRenderTargetGroup(
      std::span<RenderTarget*> color_targets,
      RenderTarget* depth_stencil_target) = 0;

  // Creates (or retrieves) a shader by name. The resource registry starts
  // the object with ref_count = 1; call Release() when done.
  [[nodiscard]] virtual Shader* CreateShader(const std::string& name) = 0;

  // Creates (or retrieves) a texture by filename (relative to data/textures/).
  // The resource registry starts the object with ref_count = 1; call Release() when done.
  [[nodiscard]] virtual Texture* CreateTexture(
      const std::string& name,
      BufferUsage usage = BufferUsage::kImmutable) = 0;

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
