#include "mesh/ObjImporter.h"

#include <cstdint>
#include <string>
#include <vector>

#include "core/BBox3.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "mesh/LodData.h"
#include "mesh/MeshData.h"
#include "mesh/MeshUtils.h"
#include "mesh/SubMeshRange.h"

// fast_obj: define implementation in this translation unit only.
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>

#include <loguru.hpp>

namespace mesh {

namespace {

// Returns the material name for the given material index, or an empty string
// when the mesh has no materials or the name is null.
std::string MaterialName(const fastObjMesh* obj, unsigned int mat_idx) {
  if (mat_idx < obj->material_count && obj->materials[mat_idx].name)
    return obj->materials[mat_idx].name;
  return {};
}

void ComputeAabb(LodData* lod) {
  if (lod->vertices.empty()) return;
  lod->aabb = core::BBox3{lod->vertices[0].position, lod->vertices[0].position};
  for (const auto& v : lod->vertices) lod->aabb << v.position;
}

}  // namespace

bool ObjImporter::Import(const std::string& path, MeshData* mesh) const {
  fastObjMesh* obj = fast_obj_read(path.c_str());
  if (!obj) {
    LOG_F(ERROR, "ObjImporter: failed to open '%s'", path.c_str());
    return false;
  }

  // fast_obj places a dummy entry at index 0 for positions, normals, texcoords.
  const bool has_normals = obj->normal_count > 1;
  const bool has_uvs     = obj->texcoord_count > 1;

  LodData& lod = mesh->lod;

  // Iterate all faces globally, grouping contiguous runs by material index
  // (face_materials[fi] gives the index for face fi).  Each run becomes one
  // SubMeshRange so that the renderer can bind the correct material per range.
  unsigned int global_index = 0;   // running offset into obj->indices
  int          cur_mat      = -1;  // current material index (-1 = none yet)
  uint32_t     sub_offset   = 0;   // index start of the current submesh

  for (unsigned int fi = 0; fi < obj->face_count; ++fi) {
    const unsigned int vert_count = obj->face_vertices[fi];
    const unsigned int mat_idx    = obj->face_materials[fi];

    if (static_cast<int>(mat_idx) != cur_mat) {
      // Close the previous submesh.
      if (cur_mat >= 0) {
        const uint32_t cnt =
            static_cast<uint32_t>(lod.indices.size()) - sub_offset;
        if (cnt > 0) {
          lod.submeshes.push_back(SubMeshRange{
              sub_offset, cnt,
              MaterialName(obj, static_cast<unsigned int>(cur_mat))});
        }
      }
      cur_mat    = static_cast<int>(mat_idx);
      sub_offset = static_cast<uint32_t>(lod.indices.size());
    }

    // Fan triangulation: (0,1,2), (0,2,3), …
    for (unsigned int ti = 1; ti + 1 < vert_count; ++ti) {
      const unsigned int corners[3] = {0u, ti, ti + 1u};
      for (unsigned int corner : corners) {
        const fastObjIndex& idx = obj->indices[global_index + corner];

        const core::Vec3f pos{
          obj->positions[3 * idx.p + 0],
          obj->positions[3 * idx.p + 1],
          obj->positions[3 * idx.p + 2],
        };
        const core::Vec3f norm = (has_normals && idx.n > 0)
            ? core::Vec3f{obj->normals[3 * idx.n + 0],
                          obj->normals[3 * idx.n + 1],
                          obj->normals[3 * idx.n + 2]}
            : core::Vec3f::kAxisY;
        const core::Vec2f uv = (has_uvs && idx.t > 0)
            ? core::Vec2f{obj->texcoords[2 * idx.t + 0],
                          obj->texcoords[2 * idx.t + 1]}
            : core::Vec2f::kZero;

        lod.indices.push_back(static_cast<uint32_t>(lod.vertices.size()));
        lod.vertices.push_back(
            core::Vertex3D{pos, norm, core::Vec3f::kZero, core::Vec3f::kZero, uv});
      }
    }
    global_index += vert_count;
  }

  // Close the last submesh.
  if (cur_mat >= 0) {
    const uint32_t cnt =
        static_cast<uint32_t>(lod.indices.size()) - sub_offset;
    if (cnt > 0) {
      lod.submeshes.push_back(SubMeshRange{
          sub_offset, cnt,
          MaterialName(obj, static_cast<unsigned int>(cur_mat))});
    }
  }

  fast_obj_destroy(obj);

  if (lod.vertices.empty()) {
    LOG_F(WARNING, "ObjImporter: no geometry found in '%s'", path.c_str());
    return false;
  }

  WeldVertices(&lod);
  if (!has_normals) ComputeNormals(&lod);
  ComputeTangents(&lod);
  ComputeAabb(&lod);

  LOG_F(INFO, "ObjImporter: loaded %zu vertices, %zu submeshes from '%s'",
        lod.vertices.size(), lod.submeshes.size(), path.c_str());
  return true;
}

}  // namespace mesh
