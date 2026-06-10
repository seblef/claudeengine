#include "mesh/MeshUtils.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Vec2f.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "mesh/LodData.h"
#include "mesh/SubMeshRange.h"

namespace mesh {

// ---- ComputeNormals ---------------------------------------------------------

void ComputeNormals(LodData* lod) {
  auto& verts   = lod->vertices;
  const auto& idx = lod->indices;
  const size_t face_count = idx.size() / 3;

  for (auto& v : verts) v.normal = core::Vec3f::kZero;

  for (size_t f = 0; f < face_count; ++f) {
    const uint32_t i0 = idx[f * 3 + 0];
    const uint32_t i1 = idx[f * 3 + 1];
    const uint32_t i2 = idx[f * 3 + 2];

    const core::Vec3f edge1 = verts[i1].position - verts[i0].position;
    const core::Vec3f edge2 = verts[i2].position - verts[i0].position;
    // Area-weighted cross product: longer edges contribute more to the average.
    const core::Vec3f weighted_normal = edge1.Cross(edge2);

    verts[i0].normal += weighted_normal;
    verts[i1].normal += weighted_normal;
    verts[i2].normal += weighted_normal;
  }

  for (auto& v : verts) {
    if (v.normal.LengthSquared() > 1e-10f)
      v.normal = v.normal.Normalized();
    else
      v.normal = core::Vec3f::kAxisY;
  }
}

// ---- ComputeTangents --------------------------------------------------------

void ComputeTangents(LodData* lod) {
  auto& verts   = lod->vertices;
  const auto& idx = lod->indices;
  const size_t n  = verts.size();
  const size_t face_count = idx.size() / 3;

  // Accumulate per-vertex tangents and bitangents in separate buffers.
  std::vector<core::Vec3f> tan(n, core::Vec3f::kZero);
  std::vector<core::Vec3f> bitan(n, core::Vec3f::kZero);

  for (size_t f = 0; f < face_count; ++f) {
    const uint32_t i0 = idx[f * 3 + 0];
    const uint32_t i1 = idx[f * 3 + 1];
    const uint32_t i2 = idx[f * 3 + 2];

    const core::Vec3f e1 = verts[i1].position - verts[i0].position;
    const core::Vec3f e2 = verts[i2].position - verts[i0].position;
    const core::Vec2f d1 = verts[i1].uv - verts[i0].uv;
    const core::Vec2f d2 = verts[i2].uv - verts[i0].uv;

    const float denom = d1.x * d2.y - d2.x * d1.y;
    if (denom == 0.f) continue;  // degenerate UV triangle

    const float r = 1.f / denom;
    const core::Vec3f t = (e1 * d2.y - e2 * d1.y) * r;
    const core::Vec3f b = (e2 * d1.x - e1 * d2.x) * r;

    tan[i0] += t;  tan[i1] += t;  tan[i2] += t;
    bitan[i0] += b; bitan[i1] += b; bitan[i2] += b;
  }

  // Gram-Schmidt orthogonalise tangent against normal; store binormal.
  for (size_t i = 0; i < n; ++i) {
    const core::Vec3f& n_  = verts[i].normal;
    const core::Vec3f& t_  = tan[i];
    const core::Vec3f& bt_ = bitan[i];

    if (t_.LengthSquared() < 1e-10f) {
      verts[i].tangent  = core::Vec3f::kAxisX;
      verts[i].binormal = core::Vec3f::kAxisZ;
      continue;
    }

    // Orthogonalise: T' = normalize(T - N * dot(N, T))
    const core::Vec3f tangent = (t_ - n_ * n_.Dot(t_)).Normalized();

    // Handedness: if (N × T) · BT < 0, flip
    const float hand = n_.Cross(tangent).Dot(bt_) < 0.f ? -1.f : 1.f;
    verts[i].tangent  = tangent;
    verts[i].binormal = n_.Cross(tangent) * hand;
  }
}

// ---- WeldVertices -----------------------------------------------------------

namespace {

// POD key for hashing a vertex by its weld-relevant fields (position + normal + UV).
// Tangent and binormal are excluded — they are computed after welding.
struct WeldKey {
  float px, py, pz;
  float nx, ny, nz;
  float u, v;

  bool operator==(const WeldKey& o) const {
    return px == o.px && py == o.py && pz == o.pz &&
           nx == o.nx && ny == o.ny && nz == o.nz &&
           u  == o.u  && v  == o.v;
  }
};

struct WeldKeyHash {
  size_t operator()(const WeldKey& k) const noexcept {
    // FNV-1a over the raw bytes — fast and collision-resistant for float data.
    constexpr size_t kFnvOffset = 14695981039346656037ULL;
    constexpr size_t kFnvPrime  = 1099511628211ULL;
    size_t h = kFnvOffset;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&k);
    for (size_t i = 0; i < sizeof(WeldKey); ++i) {
      h ^= static_cast<size_t>(p[i]);
      h *= kFnvPrime;
    }
    return h;
  }
};

WeldKey MakeKey(const core::Vertex3D& v) {
  return {v.position.x, v.position.y, v.position.z,
          v.normal.x,   v.normal.y,   v.normal.z,
          v.uv.x,       v.uv.y};
}

}  // namespace

void WeldVertices(LodData* lod) {
  const auto& src_verts = lod->vertices;
  const auto& src_idx   = lod->indices;

  std::vector<core::Vertex3D> new_verts;
  new_verts.reserve(src_verts.size());
  std::vector<uint32_t> new_idx;
  new_idx.reserve(src_idx.size());

  std::unordered_map<WeldKey, uint32_t, WeldKeyHash> seen;
  seen.reserve(src_verts.size());

  for (const uint32_t old_i : src_idx) {
    const WeldKey key = MakeKey(src_verts[old_i]);
    auto [it, inserted] = seen.emplace(key, static_cast<uint32_t>(new_verts.size()));
    if (inserted) new_verts.push_back(src_verts[old_i]);
    new_idx.push_back(it->second);
  }

  lod->vertices = std::move(new_verts);
  lod->indices  = std::move(new_idx);
}

// ---- MergeSubmeshesByMaterial -----------------------------------------------

void MergeSubmeshesByMaterial(LodData* lod) {
  if (lod->submeshes.size() <= 1) return;

  // First pass: determine first-seen material order and check if any merging
  // is actually needed (early-out when all materials are already unique).
  std::vector<std::string> order;
  order.reserve(lod->submeshes.size());
  std::unordered_map<std::string, size_t> mat_to_slot;
  mat_to_slot.reserve(lod->submeshes.size());

  for (const SubMeshRange& sub : lod->submeshes) {
    if (mat_to_slot.emplace(sub.material_name, order.size()).second)
      order.push_back(sub.material_name);
  }

  if (order.size() == lod->submeshes.size()) return;  // nothing to merge

  // Second pass: collect indices per material in first-seen order.
  std::vector<std::vector<uint32_t>> buckets(order.size());
  for (const SubMeshRange& sub : lod->submeshes) {
    const size_t slot = mat_to_slot[sub.material_name];
    auto& bucket = buckets[slot];
    const uint32_t end = sub.index_offset + sub.index_count;
    bucket.insert(bucket.end(),
                  lod->indices.begin() + sub.index_offset,
                  lod->indices.begin() + end);
  }

  // Rebuild index buffer and submesh list.
  std::vector<uint32_t> new_indices;
  new_indices.reserve(lod->indices.size());
  std::vector<SubMeshRange> new_submeshes;
  new_submeshes.reserve(order.size());

  for (size_t i = 0; i < order.size(); ++i) {
    const uint32_t offset = static_cast<uint32_t>(new_indices.size());
    new_indices.insert(new_indices.end(), buckets[i].begin(), buckets[i].end());
    new_submeshes.push_back(SubMeshRange{
        offset,
        static_cast<uint32_t>(buckets[i].size()),
        order[i]});
  }

  lod->indices   = std::move(new_indices);
  lod->submeshes = std::move(new_submeshes);
}

}  // namespace mesh
