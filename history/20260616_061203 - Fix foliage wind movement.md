# Fix: Foliage doesn't move with wind (#544)

## Problem

Foliage (grass, small plants) was completely static even when `WindSystem` was
active and pushing data into the `WindInfosBlock` (UBO slot 7) every frame.
Trees already had wind sway via `trees/tree_vs.glsl`; the foliage shaders
(`foliage/foliage_vs.glsl` and `foliage/foliage_billboard_vs.glsl`) simply did
not include the wind uniform or apply any displacement.

## Root Cause

Both foliage vertex shaders lacked `#include <uniforms/wind_infos.glsl>`, so
`wind_xz`, `wind_strength`, and `wind_time` were never sampled.  No XZ
displacement was applied to foliage vertices.

## Changes

### `data/shaders/glsl/foliage/foliage_vs.glsl`

- Added `#include <uniforms/wind_infos.glsl>`.
- Added uniforms `u_mesh_height` (object-space Y range) and `u_sway_strength`
  (peak displacement amplitude in metres).
- Decomposed `gl_Position = view_proj * w * vec4(in_position, 1.0)` into an
  explicit `world_pos = w * vec4(in_position, 1.0)` so the XZ offset can be
  applied before the projection multiply.
- Sway equation (mirrors the tree leaf sway):

  ```
  y_norm   = clamp(in_position.y / max(u_mesh_height, 0.01), 0.0, 1.0)
  sway     = sin(world_pos.x * 1.5 + wind_time * 1.8)
             * y_norm² * u_sway_strength * wind_strength / 10.0
  world_pos.xz += sway * normalize(wind_xz)
  ```

  The quadratic `y_norm²` weight keeps the base planted while the tip sways.

### `data/shaders/glsl/foliage/foliage_billboard_vs.glsl`

- Added `#include <uniforms/wind_infos.glsl>` and `uniform float u_sway_strength`.
- Applied an `in_position.y`-weighted sway to the billboard root position
  before expanding the quad, so distant billboards animate consistently with
  near meshes.

### `src/renderer/FoliageRenderer.h`

- Added `mesh_height` (`float`, default `1.f`) and `sway_strength` (`float`,
  default `0.05f`) to `LayerGPU`.

### `src/renderer/FoliageRenderer.cpp`

- In `Build()`: after loading geometry, reads `BBox3` Y range to set
  `gpu.mesh_height` (falling back to `1.f` if the mesh has no height).
- In `Render()`: sets `u_mesh_height` and `u_sway_strength` uniforms before
  each layer's instanced draw call.
- In `RenderBillboards()`: sets `u_sway_strength` before each billboard draw.

## Design Decisions

| Decision | Rationale |
|---|---|
| Reuse tree sway formula | Consistent look across the scene; same tunable parameters. |
| Default `sway_strength = 0.05` | Grass sways much less than trees (0.04–0.08 for trunks); 0.05 m tip displacement at 10 m/s wind feels natural. |
| `y_norm²` (quadratic) weight | Keeps the grass base planted; provides smooth gradient; same as `u_leaf_sway` in trees. |
| Per-layer `sway_strength` field | Allows different foliage types (tall grass vs. bushes) to have different amplitudes without shader variants. |
| Billboard sway uses `in_position.y` | Billboard quad Y ∈ [0, 1] naturally serves as height-normalized weight; no extra uniform needed. |

## Output to Keep in Mind

- `sway_strength` is currently hard-coded to `0.05f` in `FoliageRenderer.cpp`.
  If per-layer tuning is needed, `FoliageLayerDesc` should gain a
  `sway_strength` field and the editor should expose it.
- The wind sway does **not** update the bent-normal, which stays geometrically
  correct for the rest pose. This is intentional — recomputing normals per
  vertex on the GPU based on sway angle would add cost for a subtle effect.
- Billboard sway displacement is additive on top of the cylindrical rotation.
  The result is visually correct for the expected displacement range.

## Skills / Files Used

- Skills: `impl-issue`
- CLAUDE.md instructions followed: Google C++ style guide, one class per file,
  conventional commit message, cpplint pass before commit, history file.
