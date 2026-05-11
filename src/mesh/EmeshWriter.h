#pragma once

#include <string>

#include "mesh/MeshData.h"

namespace mesh {

// Serialises a MeshData to the binary .emesh format.
//
// .emesh layout (little-endian, no padding):
//   Header   : magic[4] | version u32 | submesh_count u32 | lod_count u32
//   Per submesh:
//     name_len u32 | name char[name_len]
//     Per LOD:
//       vertex_count u32 | index_count u32
//       aabb_min float[3] | aabb_max float[3]
//       vertices  VertexOnDisk[vertex_count]   (56 bytes each)
//       indices   u32[index_count]
//
// VertexOnDisk: px py pz  nx ny nz  bx by bz  tx ty tz  u v  (56 bytes)
class EmeshWriter {
 public:
  // Writes mesh to path. Returns true on success.
  // Fails if submeshes do not all carry the same number of LODs.
  bool Write(const MeshData& mesh, const std::string& path) const;
};

}  // namespace mesh
