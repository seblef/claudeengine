#include "mesh/EmeshWriter.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>

#include "mesh/MeshData.h"

#include <loguru.hpp>

namespace mesh {

namespace {

// Magic bytes identifying a .emesh file.
constexpr uint8_t kMagic[4] = {'E', 'M', 'S', 'H'};
constexpr uint32_t kVersion = 1;

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

bool EmeshWriter::Write(const MeshData& mesh, const std::string& path) const {
  if (mesh.submeshes.empty()) {
    LOG_F(WARNING, "EmeshWriter: mesh has no submeshes, skipping write to '%s'", path.c_str());
    return false;
  }

  // All submeshes must carry the same number of LODs.
  const uint32_t lod_count = static_cast<uint32_t>(mesh.submeshes[0].lods.size());
  const bool uniform = std::all_of(mesh.submeshes.begin(), mesh.submeshes.end(),
      [lod_count](const SubmeshData& s) {
        return static_cast<uint32_t>(s.lods.size()) == lod_count;
      });
  if (!uniform) {
    LOG_F(ERROR, "EmeshWriter: submesh LOD count mismatch in '%s'", path.c_str());
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
  const uint32_t submesh_count = static_cast<uint32_t>(mesh.submeshes.size());
  WriteField(out, submesh_count);
  WriteField(out, lod_count);

  for (const auto& sub : mesh.submeshes) {
    // Material slot name.
    const uint32_t name_len = static_cast<uint32_t>(sub.material_slot.size());
    WriteField(out, name_len);
    if (name_len > 0)
      out.write(sub.material_slot.data(), name_len);

    for (const auto& lod : sub.lods) {
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
    }
  }

  if (!out) {
    LOG_F(ERROR, "EmeshWriter: write error on '%s'", path.c_str());
    return false;
  }

  LOG_F(INFO, "EmeshWriter: wrote %u submesh(es), %u LOD(s) to '%s'",
        submesh_count, lod_count, path.c_str());
  return true;
}

}  // namespace mesh
