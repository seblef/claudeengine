#include "terrain/TerrainPatchMesh.h"

#include <cassert>
#include <vector>

#include "abstract/BufferUsage.h"
#include "abstract/IndexType.h"
#include "abstract/VideoDevice.h"
#include "core/VertexTerrain.h"
#include "core/VertexType.h"

namespace terrain {

TerrainPatchMesh::TerrainPatchMesh(abstract::VideoDevice* device, int patch_size) {
  assert(patch_size >= 1 && patch_size <= 255);

  const int verts_per_side = patch_size + 1;
  const int num_vertices   = verts_per_side * verts_per_side;
  const int num_indices    = patch_size * patch_size * 6;

  // Build flat XZ grid: vertex (i, j) → xz = (i, j).
  std::vector<core::VertexTerrain> vertices;
  vertices.reserve(num_vertices);
  for (int j = 0; j < verts_per_side; ++j)
    for (int i = 0; i < verts_per_side; ++i)
      vertices.emplace_back(static_cast<float>(i), static_cast<float>(j));

  // Build index list: two CCW triangles per quad.
  std::vector<uint16_t> indices;
  indices.reserve(num_indices);
  for (int j = 0; j < patch_size; ++j) {
    for (int i = 0; i < patch_size; ++i) {
      const uint16_t v00 = static_cast<uint16_t>(j * verts_per_side + i);
      const uint16_t v10 = static_cast<uint16_t>(j * verts_per_side + (i + 1));
      const uint16_t v01 = static_cast<uint16_t>((j + 1) * verts_per_side + i);
      const uint16_t v11 = static_cast<uint16_t>((j + 1) * verts_per_side + (i + 1));

      // Triangle 1: v00 → v10 → v11
      indices.push_back(v00);
      indices.push_back(v10);
      indices.push_back(v11);

      // Triangle 2: v00 → v11 → v01
      indices.push_back(v00);
      indices.push_back(v11);
      indices.push_back(v01);
    }
  }

  vbo_ = device->CreateVertexBuffer(core::VertexType::kTerrain, num_vertices,
                                    abstract::BufferUsage::kImmutable,
                                    vertices.data());
  ibo_ = device->CreateIndexBuffer(abstract::IndexType::kUInt16, num_indices,
                                   abstract::BufferUsage::kImmutable,
                                   indices.data());
  index_count_ = num_indices;
}

void TerrainPatchMesh::Bind() {
  vbo_->Bind();
  ibo_->Bind();
}

}  // namespace terrain
