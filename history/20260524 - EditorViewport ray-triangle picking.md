# EditorViewport — precise mesh selection via ray-triangle intersection

**Date:** 2026-05-24  
**Issue:** #269  
**Branch:** feat/editor-viewport-ray-triangle-picking-269  
**Depends on:** #268 (MeshTemplate CPU vertex positions)

## Summary

Replaced the single-pass bbox-only mesh pick in `EditorViewport::PickObjectAt()` with a
two-pass approach: AABB pre-filter → Möller-Trumbore ray-triangle intersection. This
eliminates false positives where clicks on empty space inside a bounding box incorrectly
selected the object.

## Changes

**`src/editor/EditorViewport.cpp`**

Three free functions added to the anonymous namespace:

- `RayTriangleIntersect(orig, dir, v0, v1, v2)` — Möller-Trumbore intersection returning
  `t >= 0` on hit, `-1.f` on miss.
- `TransformPoint(inv_world, p)` — delegates to `Vec3f::operator*(Mat4f)` which applies the
  full 4×3 transform (translation included).
- `TransformDir(inv_world, d)` — delegates to `core::TransformNoTranslation()` from `Mat4f.h`
  (upper-left 3×3 only, no translation).

`PickObjectAt()` restructured:
1. Non-mesh objects (lights, camera) are skipped entirely — lights are handled visually via
   `DrawLightsOverlay` and are not yet pickable by ray cast.
2. AABB pre-filter via `GetWorldBBox().IntersectsRay()` rejects obvious misses cheaply.
3. For mesh objects with CPU positions (file-backed), the ray is transformed into model space
   and tested against every triangle. Model-space t values are used for comparison; this is
   exact for uniform-scale transforms and approximate but sufficient for the editor for
   non-uniform scale.
4. Procedural meshes (empty `GetCPUPositions()`) fall back to the AABB hit distance.

## Decisions and rationale

**Transform the ray, not the vertices.** Avoids allocating or iterating a transformed copy
of the position array. One `Mat4f::Inverse()` per candidate object; all triangle tests run
in model space.

**Model-space t comparison.** For non-uniform scale, model-space and world-space t values
differ. The issue specification explicitly notes this approximation is "sufficient for editor
picking", so no correction factor is applied.

**Fallback for procedural meshes.** Procedural geometry (cube/sphere previews use
`__proc_` ids and have empty CPU buffers) is handled by degrading to the AABB distance,
preserving the previous behaviour for those objects.

**cpplint compliance.** Multi-statement `if` bodies required expansion to separate lines
(`[whitespace/newline]`).

## Output / notes for next features

- Issue #271 will introduce a `PickingAccelerator` spatial index to replace the linear
  `scene_->GetObjects()` iteration. The two-pass structure in `PickObjectAt()` is already
  designed around a `candidates` list and will slot in without changes to the inner logic.
- Light picking (selecting a light by clicking its wireframe overlay) is still absent. When
  implemented, it should be a separate pass after the mesh pass, likely using a small
  screen-space proximity test against the projected light position.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style guide, include root `src/`, one class per file
- `src/editor/CLAUDE.md`: editor is a leaf in the dependency graph
- `CLAUDE.md`: branch naming, conventional commits, history file requirement
