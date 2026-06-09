#include "mesh/EmeshWriter.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#include "mesh/MeshData.h"

#include <loguru.hpp>

namespace mesh {

namespace {

// Magic bytes identifying a .emesh file.
constexpr uint8_t kMagic[4] = {'E', 'M', 'S', 'H'};
constexpr uint32_t kVersion = 3;
constexpr uint32_t kMaterialPathSize = 256;

// On-disk vertex: 3 floats per Vec3f field (no 16-byte alignment padding).
struct VertexOnDisk {
  float px, py, pz;
  float nx, ny, nz;
  float bx, by, bz;
  float tx, ty, tz;
  float u, v;
};
static_assert(sizeof(VertexOnDisk) == 56, "VertexOnDisk layout changed");

template <typename T>
void WriteField(std::ofstream& out, const T& value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

}  // namespace

// cppcheck-suppress functionStatic
bool EmeshWriter::Write(const MeshData& mesh, const std::string& path) const {
  const auto& lod = mesh.lod;
  if (lod.vertices.empty()) {
    LOG_F(WARNING, "EmeshWriter: mesh has no vertices, skipping write to '%s'",
          path.c_str());
    return false;
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    LOG_F(ERROR, "EmeshWriter: cannot open '%s' for writing", path.c_str());
    return false;
  }

  // Header.
  out.write(reinterpret_cast<const char*>(kMagic), sizeof(kMagic));
  WriteField(out, kVersion);

  // Geometry.
  const uint32_t vertex_count = static_cast<uint32_t>(lod.vertices.size());
  const uint32_t index_count  = static_cast<uint32_t>(lod.indices.size());
  WriteField(out, vertex_count);
  WriteField(out, index_count);

  // AABB.
  const auto& mn = lod.aabb.GetMin();
  const auto& mx = lod.aabb.GetMax();
  WriteField(out, mn.x); WriteField(out, mn.y); WriteField(out, mn.z);
  WriteField(out, mx.x); WriteField(out, mx.y); WriteField(out, mx.z);

  // Vertices — strip the Vec3f alignment padding before writing.
  for (const auto& v : lod.vertices) {
    const VertexOnDisk vd{
      v.position.x, v.position.y, v.position.z,
      v.normal.x,   v.normal.y,   v.normal.z,
      v.binormal.x, v.binormal.y, v.binormal.z,
      v.tangent.x,  v.tangent.y,  v.tangent.z,
      v.uv.x,       v.uv.y,
    };
    out.write(reinterpret_cast<const char*>(&vd), sizeof(VertexOnDisk));
  }

  // Indices.
  if (index_count > 0)
    out.write(reinterpret_cast<const char*>(lod.indices.data()),
              index_count * sizeof(uint32_t));

  // SubMesh table.
  const uint32_t submesh_count = static_cast<uint32_t>(lod.submeshes.size());
  WriteField(out, submesh_count);
  for (const auto& sm : lod.submeshes) {
    WriteField(out, sm.index_offset);
    WriteField(out, sm.index_count);
    char mat_path[kMaterialPathSize] = {};
    const std::size_t copy_len =
        std::min(sm.material_name.size(), static_cast<std::size_t>(kMaterialPathSize - 1));
    std::memcpy(mat_path, sm.material_name.c_str(), copy_len);
    out.write(mat_path, kMaterialPathSize);
  }

  if (!out) {
    LOG_F(ERROR, "EmeshWriter: write error on '%s'", path.c_str());
    return false;
  }

  LOG_F(INFO, "EmeshWriter: wrote %u vertices, %u indices, %u submeshes to '%s'",
        vertex_count, index_count, submesh_count, path.c_str());
  return true;
}

}  // namespace mesh
