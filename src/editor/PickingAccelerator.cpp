#include "editor/PickingAccelerator.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "game/GameLight.h"
#include "game/GameObjectType.h"
#include "renderer/GlobalLight.h"
#include "renderer/Light.h"

namespace editor {

namespace {

// Returns true for objects eligible for picking acceleration.
// GlobalLight is excluded: it has no meaningful world position/bbox.
// GamePivot has a valid point bbox (min == max == world origin) and is included
// so the grid tracks its position for broad-phase picking.
bool IsPickable(const game::GameObject* obj) {
  if (obj->GetType() == game::GameObjectType::kLight) {
    const auto* gl = static_cast<const game::GameLight*>(obj);
    const renderer::Light* light = gl->GetLight();
    return light && light->GetType() != renderer::LightType::kGlobal;
  }
  return obj->GetType() == game::GameObjectType::kMesh    ||
         obj->GetType() == game::GameObjectType::kPivot   ||
         obj->GetType() == game::GameObjectType::kVehicle;
}

}  // namespace

int PickingAccelerator::CellIndex(int cx, int cy, int cz) const {
  return cx + cy * nx_ + cz * nx_ * ny_;
}

void PickingAccelerator::InsertObject(game::GameObject* obj) {
  if (!IsPickable(obj)) return;

  const core::BBox3& bbox = obj->GetWorldBBox();
  const core::Vec3f& bmn = bbox.GetMin();
  const core::Vec3f& bmx = bbox.GetMax();
  const core::Vec3f& gmn = bounds_.GetMin();

  // Clamp object bbox to grid bounds, then find overlapping cell range.
  const int x0 = std::max(0, static_cast<int>((bmn.x - gmn.x) / cell_size_));
  const int y0 = std::max(0, static_cast<int>((bmn.y - gmn.y) / cell_size_));
  const int z0 = std::max(0, static_cast<int>((bmn.z - gmn.z) / cell_size_));
  const int x1 = std::min(nx_ - 1, static_cast<int>((bmx.x - gmn.x) / cell_size_));
  const int y1 = std::min(ny_ - 1, static_cast<int>((bmx.y - gmn.y) / cell_size_));
  const int z1 = std::min(nz_ - 1, static_cast<int>((bmx.z - gmn.z) / cell_size_));

  for (int cz = z0; cz <= z1; ++cz)
    for (int cy = y0; cy <= y1; ++cy)
      for (int cx = x0; cx <= x1; ++cx)
        cells_[CellIndex(cx, cy, cz)].push_back(obj);
}

void PickingAccelerator::RemoveObject(game::GameObject* obj) {
  for (auto& cell : cells_) {
    cell.erase(std::remove(cell.begin(), cell.end(), obj), cell.end());
  }
}

void PickingAccelerator::RebuildGrid() {
  // Recompute bounds from the tracked object list.
  bool has_any = false;
  for (const game::GameObject* obj : objects_) {
    if (!IsPickable(obj)) continue;
    if (!has_any) {
      bounds_  = obj->GetWorldBBox();
      has_any  = true;
    } else {
      bounds_ << obj->GetWorldBBox();
    }
  }
  if (!has_any) return;

  const core::Vec3f size = bounds_.GetSize();
  const float diagonal = size.Length();
  cell_size_ = std::max(diagonal / 32.f, 1.f);

  nx_ = std::max(1, static_cast<int>(std::ceil(size.x / cell_size_)));
  ny_ = std::max(1, static_cast<int>(std::ceil(size.y / cell_size_)));
  nz_ = std::max(1, static_cast<int>(std::ceil(size.z / cell_size_)));

  cells_.assign(nx_ * ny_ * nz_, {});

  for (game::GameObject* obj : objects_)
    InsertObject(obj);
}

void PickingAccelerator::Build(const std::vector<game::GameObject*>& objects,
                                const core::BBox3& scene_bounds) {
  objects_ = objects;
  bounds_  = scene_bounds;

  const core::Vec3f size = bounds_.GetSize();
  const float diagonal = size.Length();
  cell_size_ = std::max(diagonal / 32.f, 1.f);

  nx_ = std::max(1, static_cast<int>(std::ceil(size.x / cell_size_)));
  ny_ = std::max(1, static_cast<int>(std::ceil(size.y / cell_size_)));
  nz_ = std::max(1, static_cast<int>(std::ceil(size.z / cell_size_)));

  cells_.assign(nx_ * ny_ * nz_, {});

  for (game::GameObject* obj : objects_)
    InsertObject(obj);
}

void PickingAccelerator::Add(game::GameObject* obj) {
  if (nx_ == 0) return;
  objects_.push_back(obj);

  // If the object's bbox fits within the current grid, insert incrementally.
  // Otherwise the grid no longer covers the full scene: rebuild from scratch.
  if (bounds_.IsCompletelyIn(obj->GetWorldBBox()))
    InsertObject(obj);
  else
    RebuildGrid();
}

void PickingAccelerator::Remove(game::GameObject* obj) {
  if (nx_ == 0) return;
  objects_.erase(std::remove(objects_.begin(), objects_.end(), obj), objects_.end());
  RemoveObject(obj);
}

void PickingAccelerator::UpdateMoved(game::GameObject* obj) {
  if (nx_ == 0) return;
  RemoveObject(obj);

  // If the object's new bbox fits within the current grid, reinsert directly.
  // Otherwise rebuild so the grid covers the expanded scene.
  if (bounds_.IsCompletelyIn(obj->GetWorldBBox()))
    InsertObject(obj);
  else
    RebuildGrid();
}

std::vector<game::GameObject*>
PickingAccelerator::QueryRay(const core::Vec3f& origin,
                              const core::Vec3f& dir) const {
  std::vector<game::GameObject*> result;
  if (nx_ == 0) return result;

  std::unordered_set<game::GameObject*> visited;

  // 3D DDA traversal.
  const core::Vec3f& gmn = bounds_.GetMin();

  // Entry point: either the ray origin (if inside the grid) or the first
  // intersection with the grid bbox.
  float t_start = 0.f;
  if (!bounds_.IntersectsRay(origin, dir, t_start)) return result;
  t_start = std::max(t_start, 0.f);

  const core::Vec3f entry = origin + dir * t_start;

  // Initial cell.
  int cx = static_cast<int>((entry.x - gmn.x) / cell_size_);
  int cy = static_cast<int>((entry.y - gmn.y) / cell_size_);
  int cz = static_cast<int>((entry.z - gmn.z) / cell_size_);
  cx = std::max(0, std::min(nx_ - 1, cx));
  cy = std::max(0, std::min(ny_ - 1, cy));
  cz = std::max(0, std::min(nz_ - 1, cz));

  // Step direction and t-delta per axis.
  const int step_x = (dir.x >= 0.f) ? 1 : -1;
  const int step_y = (dir.y >= 0.f) ? 1 : -1;
  const int step_z = (dir.z >= 0.f) ? 1 : -1;

  const float inv_dx = (std::abs(dir.x) > 1e-8f) ? (1.f / dir.x) : 1e30f;
  const float inv_dy = (std::abs(dir.y) > 1e-8f) ? (1.f / dir.y) : 1e30f;
  const float inv_dz = (std::abs(dir.z) > 1e-8f) ? (1.f / dir.z) : 1e30f;

  // t value at which the ray crosses the next cell boundary on each axis.
  const float next_bx = gmn.x + static_cast<float>(cx + (step_x > 0 ? 1 : 0)) * cell_size_;
  const float next_by = gmn.y + static_cast<float>(cy + (step_y > 0 ? 1 : 0)) * cell_size_;
  const float next_bz = gmn.z + static_cast<float>(cz + (step_z > 0 ? 1 : 0)) * cell_size_;

  float tx_max = (next_bx - origin.x) * inv_dx;
  float ty_max = (next_by - origin.y) * inv_dy;
  float tz_max = (next_bz - origin.z) * inv_dz;

  const float tx_delta = std::abs(cell_size_ * inv_dx);
  const float ty_delta = std::abs(cell_size_ * inv_dy);
  const float tz_delta = std::abs(cell_size_ * inv_dz);

  // At most nx+ny+nz steps needed to traverse the full grid.
  const int max_steps = nx_ + ny_ + nz_;

  for (int step = 0; step < max_steps; ++step) {
    if (cx < 0 || cx >= nx_ || cy < 0 || cy >= ny_ || cz < 0 || cz >= nz_) break;

    for (game::GameObject* obj : cells_[CellIndex(cx, cy, cz)]) {
      if (visited.insert(obj).second)
        result.push_back(obj);
    }

    // Advance to the next cell.
    if (tx_max < ty_max && tx_max < tz_max) {
      cx += step_x;
      tx_max += tx_delta;
    } else if (ty_max < tz_max) {
      cy += step_y;
      ty_max += ty_delta;
    } else {
      cz += step_z;
      tz_max += tz_delta;
    }
  }

  return result;
}

}  // namespace editor
