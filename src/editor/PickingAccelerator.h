#pragma once

#include <vector>

#include "core/BBox3.h"
#include "core/Vec3f.h"
#include "game/GameObject.h"

namespace editor {

// Uniform 3D grid that accelerates ray picking by pre-filtering candidates.
//
// Level 1 of the 3-level picking pipeline:
//   1. Grid cell traversal (this class) — coarse spatial filter.
//   2. BBox ray test  — per-object AABB check.
//   3. Triangle test  — exact ray-triangle intersection.
//
// Objects are inserted into every grid cell whose AABB overlaps the object's
// world bounding box. GlobalLight objects are excluded; positional lights and
// mesh objects are both supported.
//
// QueryRay() uses a 3D DDA traversal and returns deduplicated candidates
// roughly ordered by ascending depth along the ray.
class PickingAccelerator {
 public:
  // Builds the grid from scratch.
  // Cell size = max(scene_bounds.diagonal / 32, 1.f).
  void Build(const std::vector<game::GameObject*>& objects,
             const core::BBox3& scene_bounds);

  // Incremental updates — call when the scene changes.
  void Add(game::GameObject* obj);
  void Remove(game::GameObject* obj);

  // Equivalent to Remove() + Add(); call when an object's transform changes.
  void UpdateMoved(game::GameObject* obj);

  // Returns all candidate objects whose cells are intersected by the ray.
  // Result is deduplicated and roughly ordered by ascending depth along the ray.
  // May contain false positives; callers must apply bbox/triangle/screen tests.
  [[nodiscard]] std::vector<game::GameObject*>
  QueryRay(const core::Vec3f& origin, const core::Vec3f& dir) const;

  [[nodiscard]] bool IsBuilt() const { return nx_ > 0; }

 private:
  int CellIndex(int cx, int cy, int cz) const;
  void InsertObject(game::GameObject* obj);
  void RemoveObject(game::GameObject* obj);

  // Recomputes bounds from objects_, then reinitialises cell_size_/nx_/ny_/nz_/cells_.
  void RebuildGrid();

  // cppcheck-suppress unusedStructMember
  core::BBox3 bounds_;
  // cppcheck-suppress unusedStructMember
  float cell_size_ = 1.f;
  // cppcheck-suppress unusedStructMember
  int   nx_ = 0, ny_ = 0, nz_ = 0;
  // cppcheck-suppress unusedStructMember
  std::vector<std::vector<game::GameObject*>> cells_;
  // cppcheck-suppress unusedStructMember
  std::vector<game::GameObject*> objects_;
};

}  // namespace editor
