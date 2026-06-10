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
#include "mesh/SubMeshRange.h"

#include <ufbx.h>
#include <loguru.hpp>

namespace mesh {

namespace {

void ComputeAabb(LodData* lod) {
  if (lod->vertices.empty()) return;
  lod->aabb = core::BBox3{lod->vertices[0].position, lod->vertices[0].position};
  for (const auto& v : lod->vertices) lod->aabb << v.position;
}

// Returns the material name for the given material-part index, or an empty
// string when the mesh carries no material assignments.
std::string MaterialName(const ufbx_mesh* m, uint32_t part_index) {
  if (part_index < static_cast<uint32_t>(m->materials.count))
    return std::string(m->materials.data[part_index]->name.data,
                       m->materials.data[part_index]->name.length);
  return {};
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

  mesh->unit_meters = static_cast<float>(scene->settings.unit_meters);

  LodData& lod = mesh->lod;
  bool all_have_normals = true;
  std::vector<uint32_t> tri_buf;

  // Iterate nodes (not scene->meshes) so that each node's geometry_to_world
  // transform is applied to positions and normals.  Accessing scene->meshes
  // directly gives raw object-space geometry and silently drops all per-node
  // translation/rotation/scale, which causes multi-submesh FBX files to render
  // with parts in wrong positions.
  for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
    const ufbx_node* node = scene->nodes.data[ni];
    const ufbx_mesh* m = node->mesh;
    if (!m || m->num_faces == 0) continue;

    const ufbx_matrix world_mat  = node->geometry_to_world;
    const ufbx_matrix normal_mat = ufbx_matrix_for_normals(&world_mat);

    const bool has_normals = m->vertex_normal.exists;
    const bool has_uvs     = m->vertex_uv.exists;
    if (!has_normals) all_have_normals = false;

    tri_buf.resize(m->max_face_triangles * 3);

    // material_parts is always populated by ufbx (even for meshes with no
    // material assignments), so each part maps directly to a SubMeshRange.
    const size_t part_count = m->material_parts.count;
    const bool has_parts = part_count > 0;

    if (has_parts) {
      for (size_t pi = 0; pi < part_count; ++pi) {
        const ufbx_mesh_part& part = m->material_parts.data[pi];

        const uint32_t index_offset =
            static_cast<uint32_t>(lod.indices.size());

        for (size_t fi = 0; fi < part.face_indices.count; ++fi) {
          const ufbx_face face = m->faces.data[part.face_indices.data[fi]];
          const uint32_t num_tris = ufbx_triangulate_face(
              tri_buf.data(), tri_buf.size(), m, face);

          for (uint32_t ti = 0; ti < num_tris; ++ti) {
            for (uint32_t vi = 0; vi < 3; ++vi) {
              const uint32_t ix = tri_buf[ti * 3 + vi];

              const ufbx_vec3 rp = ufbx_get_vertex_vec3(&m->vertex_position, ix);
              const ufbx_vec3 rn = has_normals
                  ? ufbx_get_vertex_vec3(&m->vertex_normal, ix)
                  : ufbx_vec3{0.0, 1.0, 0.0};
              const ufbx_vec2 uv = has_uvs
                  ? ufbx_get_vertex_vec2(&m->vertex_uv, ix)
                  : ufbx_vec2{0.0, 0.0};

              const ufbx_vec3 p = ufbx_transform_position(&world_mat, rp);
              const ufbx_vec3 n = ufbx_transform_direction(&normal_mat, rn);

              lod.indices.push_back(
                  static_cast<uint32_t>(lod.vertices.size()));
              lod.vertices.push_back(core::Vertex3D{
                  {static_cast<float>(p.x), static_cast<float>(p.y),
                   static_cast<float>(p.z)},
                  {static_cast<float>(n.x), static_cast<float>(n.y),
                   static_cast<float>(n.z)},
                  core::Vec3f::kZero,
                  core::Vec3f::kZero,
                  {static_cast<float>(uv.x), static_cast<float>(uv.y)},
              });
            }
          }
        }

        const uint32_t index_count =
            static_cast<uint32_t>(lod.indices.size()) - index_offset;
        if (index_count > 0) {
          lod.submeshes.push_back(SubMeshRange{
              index_offset,
              index_count,
              MaterialName(m, part.index),
          });
        }
      }
    } else {
      // Fallback: no material_parts — treat entire mesh as one submesh.
      const uint32_t index_offset =
          static_cast<uint32_t>(lod.indices.size());

      for (uint32_t fi = 0; fi < static_cast<uint32_t>(m->num_faces); ++fi) {
        const ufbx_face face = m->faces.data[fi];
        const uint32_t num_tris = ufbx_triangulate_face(
            tri_buf.data(), tri_buf.size(), m, face);

        for (uint32_t ti = 0; ti < num_tris; ++ti) {
          for (uint32_t vi = 0; vi < 3; ++vi) {
            const uint32_t ix = tri_buf[ti * 3 + vi];

            const ufbx_vec3 rp = ufbx_get_vertex_vec3(&m->vertex_position, ix);
            const ufbx_vec3 rn = has_normals
                ? ufbx_get_vertex_vec3(&m->vertex_normal, ix)
                : ufbx_vec3{0.0, 1.0, 0.0};
            const ufbx_vec2 uv = has_uvs
                ? ufbx_get_vertex_vec2(&m->vertex_uv, ix)
                : ufbx_vec2{0.0, 0.0};

            const ufbx_vec3 p = ufbx_transform_position(&world_mat, rp);
            const ufbx_vec3 n = ufbx_transform_direction(&normal_mat, rn);

            lod.indices.push_back(
                static_cast<uint32_t>(lod.vertices.size()));
            lod.vertices.push_back(core::Vertex3D{
                {static_cast<float>(p.x), static_cast<float>(p.y),
                 static_cast<float>(p.z)},
                {static_cast<float>(n.x), static_cast<float>(n.y),
                 static_cast<float>(n.z)},
                core::Vec3f::kZero,
                core::Vec3f::kZero,
                {static_cast<float>(uv.x), static_cast<float>(uv.y)},
            });
          }
        }
      }

      const uint32_t index_count =
          static_cast<uint32_t>(lod.indices.size()) - index_offset;
      if (index_count > 0) {
        lod.submeshes.push_back(SubMeshRange{index_offset, index_count, {}});
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

  LOG_F(INFO, "FbxImporter: loaded %zu vertices, %zu submeshes from '%s'",
        lod.vertices.size(), lod.submeshes.size(), path.c_str());
  return true;
}

}  // namespace mesh
