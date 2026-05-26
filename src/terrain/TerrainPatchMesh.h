#pragma once

#include <cstdint>
#include <memory>

#include "abstract/IndexBuffer.h"
#include "abstract/VertexBuffer.h"

namespace abstract { class VideoDevice; }

namespace terrain {

// Shared flat NxN mesh for all terrain patches at every LOD level.
//
// One instance is created once and reused for all draw calls. Per-patch
// variation (origin, scale, morph factor) is supplied via constant-buffer
// uniforms in the vertex shader — not through per-vertex data.
//
// Vertex layout: VertexTerrain (XZ only, vec2 at location 0).
// Index type:    uint16 — supports patch sizes up to 255 quads per side.
//
// Usage:
//   mesh.Bind();
//   device.RenderIndexed(mesh.GetIndexCount());
class TerrainPatchMesh {
 public:
  // Builds VBO and IBO on the GPU.
  // patch_size — number of quads per side (e.g. 64). Must be in [1, 255].
  TerrainPatchMesh(abstract::VideoDevice* device, int patch_size);

  ~TerrainPatchMesh() = default;

  // Binds the VBO and IBO ready for a draw call.
  void Bind();

  // Total index count for one full-patch draw call.
  [[nodiscard]] int GetIndexCount() const { return index_count_; }

 private:
  std::unique_ptr<abstract::VertexBuffer> vbo_;
  std::unique_ptr<abstract::IndexBuffer>  ibo_;
  int                                     index_count_;
};

}  // namespace terrain
