# Visibility Culling with Octrees

**Date**: 2026-05-11  
**Milestone**: Visibility culling  
**PRs**: #104 (Plane), #105 (ViewFrustum), #106 (IVisibilitySystem + Renderable), #107 (NoCullingVisibilitySystem), #108 (OctreeVisibilitySystem), #109 (Renderer integration), #110 (Demo scene)

---

## Overview

Added full view-frustum culling to the renderer using a spatial octree. Before this milestone, all renderables were enqueued every frame regardless of visibility. After it, `Renderer::Update` builds a `ViewFrustum` from the camera VP matrix and only enqueues objects that pass an octree-accelerated frustum test, skipping entire subtrees at once.

---

## Changes by layer

### `core::Plane` (`src/core/Plane.h/.cpp`, `tests/core/PlaneTest.cpp`)

- Stores `normal_` (normalized `Vec3f`) and `dist_` (float) with equation `normal·p = dist`
- Constructors: 3-point, point+normal, dist+normal
- `Side` enum: `kBack`, `kFront`, `kOn`, `kClip`
- `Classify(Vec3f)`, `Classify(BBox3)` — tests all 8 corners
- `IntersectsLine`, `IntersectsEdge`, `operator*(Mat4f)` (inverse-transpose transform)
- Constants: `kAxisX`, `kAxisY`, `kAxisZ`

### `core::ViewFrustum` (`src/core/ViewFrustum.h/.cpp`, `tests/core/ViewFrustumTest.cpp`)

- Gribb-Hartmann extraction from a combined VP matrix
- Convention: **column-vector**, `clip = VP * p`; rows are extracted as `r3 ± r0/r1/r2`
- `ContainsPoint`, `ContainsLine` (Sutherland-Hodgman), `ContainsBBox` (per-plane AABB test)

### `renderer::IVisibilitySystem` (`src/renderer/IVisibilitySystem.h`)

- Pure-virtual interface: `AddRenderable`, `RemoveRenderable`, `OnRenderableMoved`, `CullAndEnqueue`, `Clear`

### `renderer::Renderable` (modified)

- Added `visibility_system_` pointer (set by `AddRenderable`) and `visibility_id_` (`uintptr_t` opaque handle)
- `SetWorldMatrix` calls `OnRenderableMoved` after updating `world_bbox_` — enables O(1) octree re-insertion on move
- `visibility_id_` is the key enabling O(1) operations in both systems:
  - `NoCullingVisibilitySystem`: stores vector index → O(1) swap-and-pop removal
  - `OctreeVisibilitySystem`: stores raw `OctreeNode*` → O(1) direct node access

### `renderer::NoCullingVisibilitySystem`

- Flat `vector<Renderable*>` with swap-and-pop O(1) removal
- `CullAndEnqueue` enqueues all renderables unconditionally (used for `GlobalLight` and other always-visible objects)
- Destructor resets `visibility_system_` on all registered renderables

### `renderer::OctreeVisibilitySystem`

- Standard (non-loose) octree: each object placed in deepest node whose bounds fully contain its `world_bbox_`
- `SplitNode`: creates 8 children by halving bounds; bit-manipulation maps octant index to child bounds
- `CullAndEnqueue`: DFS with `ContainsBBox` pruning — skips entire subtrees outside the frustum
- `OnRenderableMoved`: re-inserts from root only if object no longer fits in its current node
- `ComputeNumLevels(world_size, min_cell_size=10)`: returns `min(8, max(1, ceil(log2(world_size/min_cell_size))))`
- Destructor recursively resets `visibility_system_` on all renderables

### `renderer::Renderer` (modified)

- `InitVisibilitySystems(float world_size)`: creates a cube world BBox and initializes both systems; replaces any existing systems (old destructor cleans up renderable back-pointers)
- Always called in constructor with `world_size = 1000.f` — no null-guards needed
- `AddRenderable(Renderable*)`: routes to no-culling system if `IsAlwaysVisible()`, octree otherwise
- `RemoveRenderable(Renderable*)`: same routing
- `Update()`: builds `ViewFrustum(camera->GetViewProjectionMatrix())` and calls `CullAndEnqueue` on both systems before the geometry pass

### `src/app/main.cpp` (demo)

- 1000×1000×1000 world, seed=42
- 300 cube instances + 300 sphere instances at random positions in [-500,500]³, scale [0.5,4.0]
- 40 `OmniLight` instances at random positions, random RGB colors, radius [20,60]
- 1 `GlobalLight` (warm sun)
- No per-frame `Enqueue()` calls — all handled by the visibility system

---

## Key decisions

| Decision | Rationale |
|---|---|
| `visibility_id_` on `Renderable` | Enables O(1) removal and move handling without searching any container |
| Destructor calls `ClearNode` / resets back-pointers | Replacing a system via `InitVisibilitySystems` cleanly detaches all renderables |
| Always initialize in Renderer constructor | No null-guards needed anywhere in Update/Add/Remove |
| Non-loose octree (deepest fully-containing node) | Simpler than loose octree; no object stored in multiple nodes |
| `NoCullingVisibilitySystem` for `IsAlwaysVisible()` | GlobalLight spans the whole world — octree insertion would degenerate to root |

---

## Pitfalls to remember

- **`std::begin`/`std::end` on member arrays**: caused a build error with `const Plane[6]` — use pointer arithmetic (`planes_`, `planes_ + 6`) instead when the array is a class member
- **cppcheck virtual call in destructor**: calling `Clear()` (virtual) from the destructor triggers a warning; inline the cleanup directly instead
- **Gribb-Hartmann row extraction**: the engine uses column-vector convention (`clip = VP * p`), so rows are used directly (not columns). Left = `r3 + r0`, Right = `r3 - r0`, etc.
- **cppcheck `useStlAlgorithm`**: raw loops over `children` array must use `std::find_if` with `std::begin`/`std::end`
- **`ViewFrustum` forward declaration vs include**: `IVisibilitySystem.h` only forward-declares `ViewFrustum`; `.cpp` files that call `frustum.ContainsBBox(...)` must `#include "core/ViewFrustum.h"` directly
