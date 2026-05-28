# GPU-Instanced Tree Renderer with LOD and Wind Sway

## Summary

Implements `TreeLayer` (pure data, `environment` module) and `TreeRenderer`
(GPU renderer, `renderer` module) for instanced tree rendering with a 2-stage
LOD system (full mesh → billboard) and vertex-shader wind sway animation.

## Files created

| File | Purpose |
|---|---|
| `src/environment/TreeLayer.h` | Data class: density map + per-layer placement/LOD params |
| `src/environment/TreeLayer.cpp` | Constructor only (density map allocation) |
| `src/renderer/TreeRenderer.h` | Singleton GPU renderer declaration |
| `src/renderer/TreeRenderer.cpp` | Build / Render / RenderBillboards implementation |
| `data/shaders/glsl/trees/tree_vs.glsl` | Instanced mesh VS with wind sway |
| `data/shaders/glsl/trees/tree_ps.glsl` | G-buffer PS (albedo + normal + specular) |
| `data/shaders/glsl/trees/tree_billboard_vs.glsl` | Camera-facing billboard VS |
| `data/shaders/glsl/trees/tree_billboard_ps.glsl` | Alpha-tested billboard PS |

## Files modified

| File | Change |
|---|---|
| `src/environment/CMakeLists.txt` | Added `TreeLayer.cpp` |
| `src/renderer/CMakeLists.txt` | Added `TreeRenderer.cpp` |
| `src/renderer/Renderer.h` | Added `#include "renderer/TreeRenderer.h"`, `SetTreeEnabled()`, `tree_enabled_` |
| `src/renderer/Renderer.cpp` | Calls `TreeRenderer::Render()` in geometry pass, `TreeRenderer::RenderBillboards()` in emissive pass |

## Architecture decisions

### `TreeLayer` in `environment/`, `TreeRenderer` in `renderer/`

The issue suggested `environment/TreeRenderer`, but the codebase places GPU
renderers that load mesh files (via `renderer::MeshLoader` and
`renderer::GeometryData`) in the `renderer` module. The `environment` CLAUDE.md
explicitly states "must not depend on renderer". Putting `TreeRenderer` in
`renderer/` follows the exact same pattern as `FoliageRenderer` and preserves
the `environment → renderer` one-way dependency rule.

`TreeLayer` (pure data, no GPU resources) belongs to `environment/` as
specified in the issue. It mirrors `terrain::FoliageLayerDesc` + `FoliageLayer`
but lives in `environment` because trees are conceptually an environment feature.

### Instance data format (vec4, not mat4)

`FoliageRenderer` stores near instances as full `mat4` (for arbitrary TRS) and
billboard instances as `vec4`. Since trees use **uniform scale + Y translation**
with no per-instance rotation, a `vec4` (xyz=world pos, w=scale) is sufficient
for both passes. This halves SSBO memory compared to mat4 per instance.

### SSBO binding points 2 and 3

Foliage uses bindings 0 and 1. Trees use 2 and 3 to avoid collisions when both
systems are active simultaneously.

### Wind sway

- `vertex_y_norm` = vertex Y / mesh_height (auto-computed from bbox, no vertex
  colour channel needed)
- Trunk sway: `sin(world_pos.x * 0.5 + wind_time) * y_norm * trunk_sway`
- Leaf flutter: `sin(world_pos.z * 1.3 + wind_time * 2.1) * y_norm² * leaf_sway`
- Displacement scaled by `wind_strength / 10.0` and applied in XZ only
- `u_mesh_height`, `u_trunk_sway`, `u_leaf_sway` are per-draw-call uniforms set
  by `TreeRenderer::Render()` before each instanced draw

### Instance placement

Identical to `FoliageLayer::RebuildInstances()`: jittered grid weighted by
density map. Instances are scattered once in `Build()` and stored in
`all_instances_`. `ClassifyAndUpload()` partitions near/billboard per frame.

## Notes for next steps

- `TreeLayer` density-map painting (brush tools) can be added following the
  same `PaintBrush` / `EraseBrush` API as `FoliageLayer`
- YAML map loading for `tree_layer` entries (analogous to `foliage_layer`) is
  not yet implemented — add to `GameTerrain` or a dedicated loader
- The billboard texture is shared between the mesh pass albedo and the billboard
  pass; a separate dedicated billboard texture can be added later if needed
- Impostor rendering (view-from-all-angles precompute) is out of scope

## Skills and instructions used

- `src/CLAUDE.md`: one class per file, Google C++ style, include root `src/`
- `src/environment/CLAUDE.md`: no renderer dependency from environment module
- `src/terrain/CLAUDE.md`: ref for FoliageLayer placement pattern
- `history/` folder for contribution markdown
- Conventional commits for commit message
