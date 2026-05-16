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

#include <loguru.hpp>

namespace mesh {

namespace {

constexpr uint8_t kMagic[4] = {'E', 'M', 'S', 'H'};
constexpr uint32_t kSupportedVersion = 2;

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

  // Geometry header.
  uint32_t vertex_count = 0, index_count = 0;
  if (!ReadField(in, &vertex_count) || !ReadField(in, &index_count)) {
    LOG_F(ERROR, "EmeshReader: truncated geometry header in '%s'", path.c_str());
    return false;
  }

  // AABB.
  float mnx, mny, mnz, mxx, mxy, mxz;
  if (!ReadField(in, &mnx) || !ReadField(in, &mny) || !ReadField(in, &mnz) ||
      !ReadField(in, &mxx) || !ReadField(in, &mxy) || !ReadField(in, &mxz)) {
    LOG_F(ERROR, "EmeshReader: truncated AABB in '%s'", path.c_str());
    return false;
  }
  mesh->lod.aabb = core::BBox3({mnx, mny, mnz}, {mxx, mxy, mxz});

  // Vertices.
  mesh->lod.vertices.resize(vertex_count);
  for (auto& v : mesh->lod.vertices) {
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
  mesh->lod.indices.resize(index_count);
  if (index_count > 0) {
    in.read(reinterpret_cast<char*>(mesh->lod.indices.data()),
            index_count * sizeof(uint32_t));
    if (!in) {
      LOG_F(ERROR, "EmeshReader: truncated index data in '%s'", path.c_str());
      return false;
    }
  }

  LOG_F(INFO, "EmeshReader: loaded %u vertices, %u indices from '%s'",
        vertex_count, index_count, path.c_str());
  return true;
}

}  // namespace mesh
