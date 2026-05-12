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
#include "mesh/SubmeshData.h"

// fast_obj: define implementation in this translation unit only.
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>

#include <loguru.hpp>

namespace mesh {

namespace {

// Returns the material slot name for the first face of a group, or empty.
std::string MaterialSlotName(const fastObjMesh* obj, unsigned int face_offset) {
  if (!obj->face_materials || obj->material_count == 0)
    return {};
  const unsigned int mat_idx = obj->face_materials[face_offset];
  if (mat_idx >= obj->material_count || !obj->materials[mat_idx].name)
    return {};
  return obj->materials[mat_idx].name;
}

// Builds the global index offset into obj->indices for the first vertex of
// faces[face_offset].
unsigned int GlobalVertexOffset(const fastObjMesh* obj, unsigned int face_offset) {
  unsigned int offset = 0;
  for (unsigned int fi = 0; fi < face_offset; ++fi)
    offset += obj->face_vertices[fi];
  return offset;
}

// Appends the triangulated geometry of one OBJ group into lod.
// Vertices are pushed sequentially (indices 0, 1, 2, …) to be welded later.
void AppendGroupGeometry(const fastObjMesh* obj, const fastObjGroup& grp,
                         bool has_normals, bool has_uvs, LodData* lod) {
  const unsigned int global_vertex_start = GlobalVertexOffset(obj, grp.face_offset);
  unsigned int local_offset = 0;

  for (unsigned int fi = 0; fi < grp.face_count; ++fi) {
    const unsigned int face_idx  = grp.face_offset + fi;
    const unsigned int vert_count = obj->face_vertices[face_idx];

    // Fan triangulation: (0,1,2), (0,2,3), …
    for (unsigned int ti = 1; ti + 1 < vert_count; ++ti) {
      const unsigned int corners[3] = {0u, ti, ti + 1u};
      for (unsigned int corner : corners) {
        const fastObjIndex& idx =
            obj->indices[global_vertex_start + local_offset + corner];

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

        const uint32_t new_idx = static_cast<uint32_t>(lod->vertices.size());
        lod->vertices.push_back(
            core::Vertex3D{pos, norm, core::Vec3f::kZero, core::Vec3f::kZero, uv});
        lod->indices.push_back(new_idx);
      }
    }
    local_offset += vert_count;
  }
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

  const unsigned int group_count = obj->group_count;
  mesh->submeshes.reserve(group_count);

  for (unsigned int g = 0; g < group_count; ++g) {
    const fastObjGroup& grp = obj->groups[g];
    if (grp.face_count == 0) continue;

    SubmeshData& sub = mesh->submeshes.emplace_back();
    sub.material_slot = MaterialSlotName(obj, grp.face_offset);

    LodData& lod = sub.lods.emplace_back();
    AppendGroupGeometry(obj, grp, has_normals, has_uvs, &lod);

    WeldVertices(&lod);
    if (!has_normals) ComputeNormals(&lod);
    ComputeTangents(&lod);
    ComputeAabb(&lod);
  }

  fast_obj_destroy(obj);

  if (mesh->submeshes.empty()) {
    LOG_F(WARNING, "ObjImporter: no geometry found in '%s'", path.c_str());
    return false;
  }

  LOG_F(INFO, "ObjImporter: loaded %zu submesh(es) from '%s'",
        mesh->submeshes.size(), path.c_str());
  return true;
}

}  // namespace mesh
