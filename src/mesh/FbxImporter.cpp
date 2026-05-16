#include "mesh/FbxImporter.h"

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

#include <ufbx.h>
#include <loguru.hpp>

namespace mesh {

namespace {

void ComputeAabb(LodData* lod) {
  if (lod->vertices.empty()) return;
  lod->aabb = core::BBox3{lod->vertices[0].position, lod->vertices[0].position};
  for (const auto& v : lod->vertices) lod->aabb << v.position;
}

}  // namespace

bool FbxImporter::Import(const std::string& path, MeshData* mesh) const {
  ufbx_load_opts opts{};
  ufbx_error error{};
  ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);
  if (!scene) {
    LOG_F(ERROR, "FbxImporter: failed to load '%s': %s",
          path.c_str(), error.description.data);
    return false;
  }

  // Triangulation scratch buffer.
  std::vector<uint32_t> tri_buf;

  // Merge all mesh objects and all their faces into a single LodData.
  // Material parts are ignored — materials are a game-level concept.
  LodData& lod = mesh->lod;
  bool all_have_normals = true;

  for (size_t mi = 0; mi < scene->meshes.count; ++mi) {
    const ufbx_mesh* m = scene->meshes.data[mi];
    if (m->num_faces == 0) continue;

    const bool has_normals = m->vertex_normal.exists;
    const bool has_uvs     = m->vertex_uv.exists;
    if (!has_normals) all_have_normals = false;

    tri_buf.resize(m->max_face_triangles * 3);

    for (uint32_t fi = 0; fi < static_cast<uint32_t>(m->num_faces); ++fi) {
      const ufbx_face face = m->faces.data[fi];
      const uint32_t num_tris = ufbx_triangulate_face(
          tri_buf.data(), tri_buf.size(), m, face);

      for (uint32_t ti = 0; ti < num_tris; ++ti) {
        for (uint32_t vi = 0; vi < 3; ++vi) {
          const uint32_t ix = tri_buf[ti * 3 + vi];

          const ufbx_vec3 p = ufbx_get_vertex_vec3(&m->vertex_position, ix);
          const ufbx_vec3 n = has_normals
              ? ufbx_get_vertex_vec3(&m->vertex_normal, ix)
              : ufbx_vec3{0.0, 1.0, 0.0};
          const ufbx_vec2 uv = has_uvs
              ? ufbx_get_vertex_vec2(&m->vertex_uv, ix)
              : ufbx_vec2{0.0, 0.0};

          const uint32_t new_idx = static_cast<uint32_t>(lod.vertices.size());
          lod.vertices.push_back(core::Vertex3D{
              {static_cast<float>(p.x),  static_cast<float>(p.y),  static_cast<float>(p.z)},
              {static_cast<float>(n.x),  static_cast<float>(n.y),  static_cast<float>(n.z)},
              core::Vec3f::kZero,
              core::Vec3f::kZero,
              {static_cast<float>(uv.x), static_cast<float>(uv.y)},
          });
          lod.indices.push_back(new_idx);
        }
      }
    }
  }

  ufbx_free_scene(scene);

  if (lod.vertices.empty()) {
    LOG_F(WARNING, "FbxImporter: no geometry found in '%s'", path.c_str());
    return false;
  }

  WeldVertices(&lod);
  if (!all_have_normals) ComputeNormals(&lod);
  ComputeTangents(&lod);
  ComputeAabb(&lod);

  LOG_F(INFO, "FbxImporter: loaded %zu vertices from '%s'",
        lod.vertices.size(), path.c_str());
  return true;
}

}  // namespace mesh
