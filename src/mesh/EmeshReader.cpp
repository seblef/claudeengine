#include "mesh/EmeshReader.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>

#include "core/BBox3.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "mesh/LodData.h"
#include "mesh/MeshData.h"
#include "mesh/SubmeshData.h"

#include <loguru.hpp>

namespace mesh {

namespace {

constexpr uint8_t kMagic[4] = {'E', 'M', 'S', 'H'};
constexpr uint32_t kSupportedVersion = 1;

struct VertexOnDisk {
  float px, py, pz;
  float nx, ny, nz;
  float bx, by, bz;
  float tx, ty, tz;
  float u, v;
};
static_assert(sizeof(VertexOnDisk) == 56, "VertexOnDisk layout changed");

template <typename T>
bool ReadField(std::ifstream& in, T* value) {
  in.read(reinterpret_cast<char*>(value), sizeof(T));
  return in.good();
}

}  // namespace

bool EmeshReader::Read(const std::string& path, MeshData* mesh) const {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    LOG_F(ERROR, "EmeshReader: cannot open '%s'", path.c_str());
    return false;
  }

  // Magic.
  uint8_t magic[4];
  in.read(reinterpret_cast<char*>(magic), 4);
  if (!in || std::memcmp(magic, kMagic, 4) != 0) {
    LOG_F(ERROR, "EmeshReader: bad magic in '%s'", path.c_str());
    return false;
  }

  // Version.
  uint32_t version = 0;
  if (!ReadField(in, &version) || version != kSupportedVersion) {
    LOG_F(ERROR, "EmeshReader: unsupported version %u in '%s'", version, path.c_str());
    return false;
  }

  uint32_t submesh_count = 0, lod_count = 0;
  if (!ReadField(in, &submesh_count) || !ReadField(in, &lod_count)) {
    LOG_F(ERROR, "EmeshReader: truncated header in '%s'", path.c_str());
    return false;
  }

  mesh->submeshes.resize(submesh_count);

  for (auto& sub : mesh->submeshes) {
    // Material slot name.
    uint32_t name_len = 0;
    if (!ReadField(in, &name_len)) {
      LOG_F(ERROR, "EmeshReader: truncated submesh name length in '%s'", path.c_str());
      return false;
    }
    sub.material_slot.resize(name_len);
    if (name_len > 0) {
      in.read(sub.material_slot.data(), name_len);
      if (!in) {
        LOG_F(ERROR, "EmeshReader: truncated submesh name in '%s'", path.c_str());
        return false;
      }
    }

    sub.lods.resize(lod_count);
    for (auto& lod : sub.lods) {
      uint32_t vertex_count = 0, index_count = 0;
      if (!ReadField(in, &vertex_count) || !ReadField(in, &index_count)) {
        LOG_F(ERROR, "EmeshReader: truncated LOD header in '%s'", path.c_str());
        return false;
      }

      // AABB.
      float mnx, mny, mnz, mxx, mxy, mxz;
      if (!ReadField(in, &mnx) || !ReadField(in, &mny) || !ReadField(in, &mnz) ||
          !ReadField(in, &mxx) || !ReadField(in, &mxy) || !ReadField(in, &mxz)) {
        LOG_F(ERROR, "EmeshReader: truncated AABB in '%s'", path.c_str());
        return false;
      }
      lod.aabb = core::BBox3({mnx, mny, mnz}, {mxx, mxy, mxz});

      // Vertices.
      lod.vertices.resize(vertex_count);
      for (auto& v : lod.vertices) {
        VertexOnDisk vd{};
        in.read(reinterpret_cast<char*>(&vd), sizeof(VertexOnDisk));
        if (!in) {
          LOG_F(ERROR, "EmeshReader: truncated vertex data in '%s'", path.c_str());
          return false;
        }
        v = core::Vertex3D(
          {vd.px, vd.py, vd.pz},
          {vd.nx, vd.ny, vd.nz},
          {vd.bx, vd.by, vd.bz},
          {vd.tx, vd.ty, vd.tz},
          {vd.u,  vd.v});
      }

      // Indices.
      lod.indices.resize(index_count);
      if (index_count > 0) {
        in.read(reinterpret_cast<char*>(lod.indices.data()),
                index_count * sizeof(uint32_t));
        if (!in) {
          LOG_F(ERROR, "EmeshReader: truncated index data in '%s'", path.c_str());
          return false;
        }
      }
    }
  }

  LOG_F(INFO, "EmeshReader: loaded %u submesh(es), %u LOD(s) from '%s'",
        submesh_count, lod_count, path.c_str());
  return true;
}

}  // namespace mesh
