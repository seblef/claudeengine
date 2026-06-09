#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "abstract/IndexBuffer.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/BBox3.h"
#include "core/Vertex3D.h"
#include "mesh/SubMeshRange.h"

namespace renderer {

// Immutable bundle of 3D geometry: a vertex buffer (k3D layout), a 32-bit
// index buffer, the axis-aligned bounding box of the vertex positions, and an
// optional list of submesh ranges.
//
// Vertex type is fixed to core::VertexType::k3D; index type to kUInt32.
// Both buffers are created as kImmutable at construction time.
// The bounding box is computed automatically from the supplied vertex data.
//
// When `submeshes` is empty the geometry is treated as a single material
// covering the full index range (legacy behaviour).
class GeometryData {
 public:
  // Creates the vertex and index buffers on `video` and computes the bbox.
  // `video` must outlive this object.
  // `num_triangles` is the number of triangles; `indices` must contain
  // num_triangles * 3 entries.
  // `submeshes` may be empty for single-material meshes.
  GeometryData(abstract::VideoDevice* video,
               int num_vertices, const core::Vertex3D* vertices,
               int num_triangles, const uint32_t* indices,
               std::vector<mesh::SubMeshRange> submeshes = {});

  GeometryData(const GeometryData&)            = delete;
  GeometryData& operator=(const GeometryData&) = delete;
  GeometryData(GeometryData&&)                 = delete;
  GeometryData& operator=(GeometryData&&)      = delete;

  // Binds the vertex and index buffers for subsequent indexed draw calls.
  void Set() const;

  [[nodiscard]] int                        GetNumTriangles()   const { return num_triangles_; }
  [[nodiscard]] int                        GetNumIndices()     const { return num_triangles_ * 3; }
  [[nodiscard]] abstract::VertexBuffer*    GetVertexBuffer()   const { return vertex_buffer_.get(); }
  [[nodiscard]] abstract::IndexBuffer*     GetIndexBuffer()    const { return index_buffer_.get(); }
  [[nodiscard]] const core::BBox3&         GetBBox()           const { return bbox_; }

  // Returns the number of submeshes.  Returns 0 when the geometry has no
  // explicit submesh ranges (single-material / legacy mode).
  [[nodiscard]] int GetSubMeshCount() const {
    return static_cast<int>(submeshes_.size());
  }

  // Returns the i-th submesh range.  `i` must be in [0, GetSubMeshCount()).
  [[nodiscard]] const mesh::SubMeshRange& GetSubMesh(int i) const {
    return submeshes_[i];
  }

 private:
  std::unique_ptr<abstract::VertexBuffer> vertex_buffer_;
  std::unique_ptr<abstract::IndexBuffer>  index_buffer_;
  core::BBox3                             bbox_;
  int                                     num_triangles_;
  std::vector<mesh::SubMeshRange>         submeshes_;
};

}  // namespace renderer
