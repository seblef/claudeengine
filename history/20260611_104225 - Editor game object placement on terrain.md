# Editor: Game Object Placement on Terrain (#467)

## Summary

When a terrain is present in the scene, new game objects are now placed on the
terrain surface under the mouse cursor instead of on the y=0 floor plane. The
fallback to the floor plane is preserved when no terrain exists or when the ray
misses the terrain bounds.

## Changes

**`src/editor/EditorViewport.cpp`**

Two placement paths were updated:

1. **`UpdatePreviewPosition`** — drives the live "ghost" preview that follows the
   cursor when creating meshes, lights, player starts, and particle systems.
   Previously it ray-cast to the y=0 floor plane and placed the preview at
   `{hit.x, preview_height_, hit.z}`. Now it delegates to the existing
   `ComputeTerrainHit`, which already handles both the terrain ray-march and the
   floor-plane fallback. The placement Y becomes `hit.y + preview_height_`, so
   lights (height=10) still float 10 units above the terrain surface.

2. **`PlaceMeshAt`** — handles drag-and-drop placement from the Resources panel.
   Previously it ray-cast to the floor plane and placed the mesh at y=0.
   Now it also delegates to `ComputeTerrainHit` and places the mesh at the full
   returned hit point `{hit.x, hit.y, hit.z}`.

## Design decisions

- **Reuse `ComputeTerrainHit`**: this method already implements the ray-march
  + binary-search refinement against the heightmap *and* the floor-plane fallback.
  Inlining duplicated ray logic in both callers was eliminated entirely, reducing
  the diff to 9 net lines added and ~45 deleted.
- **No slope alignment**: the issue explicitly forbids aligning the bounding box
  to the terrain slope. Objects are placed upright (identity rotation); only the
  Y position is snapped to the terrain height.
- **Backward compatible**: when `terrain_data_` is null, `ComputeTerrainHit`
  returns a y=0 floor-plane hit, so behaviour is unchanged for scenes without a
  terrain.

## Notes for future work

- The ray-march in `ComputeTerrainHit` uses 64 steps over 2000 world units,
  which is sufficient for typical terrains. Steeper slopes or very close cameras
  may benefit from finer steps.
- The same `ComputeTerrainHit` pattern could be applied to the "Fall to Terrain"
  toolbar action (already implemented in #465) and any future snap-to-terrain
  operations.

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md`: editor is the dependency-graph leaf; ImGui lifecycle rules
- Root `CLAUDE.md`: conventional commits, `cpplint` before commit, history file required
