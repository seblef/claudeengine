#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "abstract/RawTexture.h"
#include "abstract/CompareFunc.h"
#include "abstract/ConstantBuffer.h"
#include "abstract/CullFace.h"
#include "abstract/Face.h"
#include "abstract/GeometryBuffer.h"
#include "abstract/IndexBuffer.h"
#include "abstract/IndexType.h"
#include "abstract/PrimitiveType.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetCube.h"
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

  // Sets the depth comparison function. Defaults to kLess after context creation.
  // Use kLessEqual for the emissive pass so fragments at the same depth as the
  // geometry pass are accepted.
  virtual void SetDepthFunc(CompareFunc func) = 0;

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

  // Enables or disables blending. When enabled, the blend equation is:
  //   result = fragment_color * src + framebuffer_color * dst
  // src and dst are ignored when enabled is false.
  virtual void SetBlendEnabled(bool enabled,
                               BlendFactor src = BlendFactor::kOne,
                               BlendFactor dst = BlendFactor::kZero) = 0;

  // Controls whether draw calls write to the depth buffer (glDepthMask).
  // Disable before a pass that reads depth but must not overwrite it.
  // Note: glClear(GL_DEPTH_BUFFER_BIT) also respects this mask.
  virtual void SetDepthWriteEnabled(bool enabled) = 0;

  // Sets the rendering viewport to the given rectangle.
  // Use before rendering into a shadow map (non-screen-sized target).
  // BeginFrame() resets the viewport to the full screen dimensions.
  virtual void SetViewport(int x, int y, int width, int height) = 0;

  // Unbinds any texture currently bound to the given sampler slot.
  // Call this before writing to a texture that was previously read as a sampler,
  // to avoid the OpenGL texture feedback loop (undefined behavior).
  virtual void UnbindSampler(int slot) = 0;

  // Creates an off-screen render target texture of the given format.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<RenderTarget> CreateRenderTarget(
      int width, int height, TextureFormat format) = 0;

  // Creates a cube-map depth render target for omni-light shadow rendering.
  // The only supported format is kDepth32F.
  // The caller owns the returned object exclusively (unique_ptr).
  [[nodiscard]] virtual std::unique_ptr<RenderTargetCube> CreateRenderTargetCube(
      int size, TextureFormat format) = 0;

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

  // Creates a 16-bit unsigned-normalised single-channel (R16) texture from
  // raw CPU data. width * height uint16_t values; samples map to [0, 1] in
  // shaders. Intended for heightmaps uploaded at terrain init time.
  // The caller owns the returned object via unique_ptr.
  [[nodiscard]] virtual std::unique_ptr<RawTexture> CreateHeightmapTexture(
      int width, int height, const uint16_t* data) = 0;

  // Creates an RGBA8 texture from raw CPU data for use as a terrain normal map.
  // data must point to width * height * 4 bytes (R, G, B, A per texel).
  // Supports UpdateRegion() for incremental dirty-tile re-uploads.
  // The caller owns the returned object via unique_ptr.
  [[nodiscard]] virtual std::unique_ptr<RawTexture> CreateNormalMapTexture(
      int width, int height, const uint8_t* data) = 0;

  // Loads an RGBA8 PNG image from the given absolute path into a CPU pixel buffer.
  // Returns true and fills *out_width, *out_height, and out_pixels on success.
  // Returns false without logging when the file does not exist.
  // Logs an error and returns false if the file exists but cannot be decoded.
  [[nodiscard]] virtual bool LoadRGBA8File(
      const std::filesystem::path& path,
      int* out_width, int* out_height,
      std::vector<uint8_t>& out_pixels) = 0;

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
