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
#include "mesh/SubmeshData.h"

#include <ufbx.h>
#include <loguru.hpp>

namespace mesh {

namespace {

// Returns the material slot name for the first face of a mesh part, or empty.
std::string MaterialSlotName(const ufbx_mesh* m, const ufbx_mesh_part& part) {
  if (part.face_indices.count == 0 || m->face_material.count == 0)
    return {};
  const uint32_t face_idx = part.face_indices.data[0];
  const uint32_t mat_idx  = m->face_material.data[face_idx];
  if (mat_idx >= m->materials.count || !m->materials.data[mat_idx])
    return {};
  const ufbx_string& name = m->materials.data[mat_idx]->name;
  return std::string(name.data, name.length);
}

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

  // Triangulation scratch buffer — sized per mesh inside the loop.
  std::vector<uint32_t> tri_buf;

  for (size_t mi = 0; mi < scene->meshes.count; ++mi) {
    const ufbx_mesh* m = scene->meshes.data[mi];
    if (m->num_faces == 0) continue;

    const bool has_normals = m->vertex_normal.exists;
    const bool has_uvs     = m->vertex_uv.exists;

    tri_buf.resize(m->max_face_triangles * 3);

    const size_t part_count = m->material_parts.count;
    const size_t effective  = part_count > 0 ? part_count : 1;

    for (size_t pi = 0; pi < effective; ++pi) {
      // Face range: either from a material part or the whole mesh.
      size_t num_part_faces = 0;
      const uint32_t* face_idx_data = nullptr;
      if (part_count > 0) {
        const ufbx_mesh_part& part = m->material_parts.data[pi];
        if (part.num_faces == 0) continue;
        num_part_faces = part.face_indices.count;
        face_idx_data  = part.face_indices.data;
      } else {
        num_part_faces = m->num_faces;
        // Build a sequential range on the fly — handled below via branch.
      }

      SubmeshData& sub = mesh->submeshes.emplace_back();
      if (part_count > 0)
        sub.material_slot = MaterialSlotName(m, m->material_parts.data[pi]);

      LodData& lod = sub.lods.emplace_back();

      auto AppendFace = [&](uint32_t fi) {
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

            const uint32_t new_idx =
                static_cast<uint32_t>(lod.vertices.size());
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
      };

      if (part_count > 0) {
        for (size_t fi = 0; fi < num_part_faces; ++fi)
          AppendFace(face_idx_data[fi]);
      } else {
        for (uint32_t fi = 0; fi < static_cast<uint32_t>(m->num_faces); ++fi)
          AppendFace(fi);
      }

      WeldVertices(&lod);
      if (!has_normals) ComputeNormals(&lod);
      ComputeTangents(&lod);
      ComputeAabb(&lod);
    }
  }

  ufbx_free_scene(scene);

  if (mesh->submeshes.empty()) {
    LOG_F(WARNING, "FbxImporter: no geometry found in '%s'", path.c_str());
    return false;
  }

  LOG_F(INFO, "FbxImporter: loaded %zu submesh(es) from '%s'",
        mesh->submeshes.size(), path.c_str());
  return true;
}

}  // namespace mesh
