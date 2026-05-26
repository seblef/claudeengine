#include "terrain/CDLODQuadTree.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "core/Camera.h"
#include "core/ViewFrustum.h"
#include "terrain/TerrainData.h"

namespace terrain {

namespace {

constexpr float kPi         = 3.14159265358979323846f;
// Morph begins at this fraction of each LOD's maximum distance.
constexpr float kMorphStart = 0.8f;

}  // namespace

// -----------------------------------------------------------------------------
// Build
// -----------------------------------------------------------------------------

void CDLODQuadTree::Build(const TerrainData& data, int patch_size,
                          int lod_count) {
  assert(patch_size > 0);
  assert(lod_count  > 0);

  nodes_.clear();
  roots_.clear();
  patch_size_       = patch_size;
  lod_count_        = lod_count;
  meters_per_texel_ =
      data.GetWorldWidth() / static_cast<float>(data.GetTexelWidth());

  // Top-level LOD: each root node covers patch_size * 2^(lod_count-1) texels.
  const int top_lod         = lod_count - 1;
  const int top_tile_texels = patch_size * (1 << top_lod);
  const int grid_w =
      (data.GetTexelWidth()  + top_tile_texels - 1) / top_tile_texels;
  const int grid_h =
      (data.GetTexelHeight() + top_tile_texels - 1) / top_tile_texels;

  roots_.reserve(grid_w * grid_h);
  for (int gz = 0; gz < grid_h; ++gz) {
    for (int gx = 0; gx < grid_w; ++gx) {
      roots_.push_back(BuildNode(data, top_lod, gx, gz));
    }
  }
}

int CDLODQuadTree::BuildNode(const TerrainData& data, int lod,
                              int grid_x, int grid_z) {
  const int   tile_texels = patch_size_ * (1 << lod);
  const float world_size  = static_cast<float>(tile_texels) * meters_per_texel_;
  const float x_min       = static_cast<float>(grid_x) * world_size;
  const float z_min       = static_cast<float>(grid_z) * world_size;
  const float x_max       = x_min + world_size;
  const float z_max       = z_min + world_size;

  // Reserve a slot first; use index-based access afterwards so that
  // recursive calls that grow nodes_ don't invalidate our reference.
  const int idx = static_cast<int>(nodes_.size());
  nodes_.emplace_back();
  nodes_[idx].lod    = lod;
  nodes_[idx].grid_x = grid_x;
  nodes_[idx].grid_z = grid_z;
  for (int i = 0; i < 4; ++i) nodes_[idx].children[i] = -1;

  if (lod == 0) {
    // Leaf: sample all (patch_size+1)^2 vertex positions for exact Y bounds.
    float y_min =  std::numeric_limits<float>::max();
    float y_max = -std::numeric_limits<float>::max();
    for (int sx = 0; sx <= patch_size_; ++sx) {
      for (int sz = 0; sz <= patch_size_; ++sz) {
        const float h = data.GetHeight(
            x_min + static_cast<float>(sx) * meters_per_texel_,
            z_min + static_cast<float>(sz) * meters_per_texel_);
        y_min = std::min(y_min, h);
        y_max = std::max(y_max, h);
      }
    }
    nodes_[idx].aabb =
        core::BBox3{{x_min, y_min, z_min}, {x_max, y_max, z_max}};
  } else {
    // Interior: build 2×2 children at lod-1; derive Y bounds from their AABBs.
    const int child_lod         = lod - 1;
    const int child_tile_texels = patch_size_ * (1 << child_lod);
    const int child_grid_w =
        (data.GetTexelWidth()  + child_tile_texels - 1) / child_tile_texels;
    const int child_grid_h =
        (data.GetTexelHeight() + child_tile_texels - 1) / child_tile_texels;

    float y_min =  std::numeric_limits<float>::max();
    float y_max = -std::numeric_limits<float>::max();

    int ci = 0;
    for (int dz = 0; dz < 2; ++dz) {
      for (int dx = 0; dx < 2; ++dx) {
        const int cgx = grid_x * 2 + dx;
        const int cgz = grid_z * 2 + dz;
        if (cgx < child_grid_w && cgz < child_grid_h) {
          const int child_idx = BuildNode(data, child_lod, cgx, cgz);
          nodes_[idx].children[ci] = child_idx;
          y_min = std::min(y_min, nodes_[child_idx].aabb.GetMin().y);
          y_max = std::max(y_max, nodes_[child_idx].aabb.GetMax().y);
        }
        ++ci;
      }
    }

    if (y_min > y_max) y_min = y_max = 0.f;  // guard for degenerate edge tiles
    nodes_[idx].aabb =
        core::BBox3{{x_min, y_min, z_min}, {x_max, y_max, z_max}};
  }

  return idx;
}

// -----------------------------------------------------------------------------
// Select
// -----------------------------------------------------------------------------

void CDLODQuadTree::Select(const core::Camera& camera, int max_triangles,
                            std::vector<TerrainPatch>& out) const {
  out.clear();

  // Derive LOD switch distances from the triangle budget.
  //
  // Assuming the terrain is viewed from above, each LOD l ring contributes
  // approximately 2π*(d_0/mpt)² triangles, independent of l (ring areas cancel
  // out with patch sizes).  Summing over lod_count levels:
  //   total ≈ lod_count * 2π * (d_0 / mpt)²  =  max_triangles
  //   d_0 = mpt * sqrt(max_triangles / (2π * lod_count))
  //   d_l = d_0 * 2^l
  const float d0 =
      meters_per_texel_ *
      std::sqrt(static_cast<float>(max_triangles) /
                (2.f * kPi * static_cast<float>(lod_count_)));

  float lod_ranges[32] = {};  // lod_count_ is always small; 32 is a safe upper bound
  for (int l = 0; l < lod_count_; ++l) {
    lod_ranges[l] = d0 * static_cast<float>(1 << l);
  }

  const core::ViewFrustum frustum{camera.GetViewProjectionMatrix()};
  const core::Vec3f       cam_pos = camera.GetPosition();

  for (int root : roots_) {
    SelectNode(root, frustum, cam_pos, lod_ranges, out);
  }
}

void CDLODQuadTree::SelectNode(int idx, const core::ViewFrustum& frustum,
                                const core::Vec3f& cam_pos,
                                const float* lod_ranges,
                                std::vector<TerrainPatch>& out) const {
  const Node& node = nodes_[idx];

  if (!frustum.ContainsBBox(node.aabb)) return;

  const float dist = node.aabb.Distance(cam_pos);
  const int   lod  = node.lod;

  // Split into children when the camera is close enough to warrant finer detail
  // and children exist (i.e., not a leaf or a terrain-edge node with no kids).
  if (lod > 0 && dist < lod_ranges[lod - 1]) {
    for (int i = 0; i < 4; ++i) {
      if (node.children[i] >= 0) {
        SelectNode(node.children[i], frustum, cam_pos, lod_ranges, out);
      }
    }
    return;
  }

  // Emit this patch.  Compute a morph blend toward the next coarser LOD to
  // prevent popping at LOD boundaries.  The morph ramps from 0 to 1 over the
  // last 20 % of this LOD's distance range.  The coarsest LOD never morphs
  // (there is no coarser level to blend to).
  float morph = 0.f;
  if (lod < lod_count_ - 1) {
    const float range_end   = lod_ranges[lod];
    const float range_start = kMorphStart * range_end;
    if (dist > range_start) {
      morph = (dist - range_start) / (range_end - range_start);
      morph = std::min(morph, 1.f);
    }
  }

  out.push_back({lod, node.grid_x, node.grid_z, morph});
}

}  // namespace terrain
