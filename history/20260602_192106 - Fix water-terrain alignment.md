# Fix: water–terrain alignment (#373)

## Problem

When water was enabled, the water plane did not cover the terrain correctly.
The water mesh was built as a square centred at the world-space origin (0, 0, 0),
while the terrain is positioned with its corner at the origin and extends to
`(world_width, world_height)` in XZ. This left a large portion of the terrain
without water and made the water extend equally in the negative-X/Z direction
where there is no terrain.

## Root cause

`WaterRenderer::BuildMesh()` computed vertex positions as:

```
x = -half + i * kCellSize   (half = grid_size * kCellSize / 2)
z = -half + j * kCellSize
```

with no concept of where the terrain actually sits in world space.

## Fix

Three files were changed:

### `src/environment/WaterRenderer.h` / `WaterRenderer.cpp`

- `Build()` now accepts two optional parameters: `terrain_world_width` and
  `terrain_world_height` (both default to 0 → legacy behaviour, fixed grid
  centred at the origin).
- A new constant `kTerrainMarginFactor = 1.2f` defines how much the water
  extends beyond each terrain edge (20 %).
- In `BuildMesh()`, when non-zero terrain dimensions are supplied:
  - The centre of the mesh is shifted to `(terrain_world_width / 2,
    terrain_world_height / 2)`.
  - The half-extent becomes `max(width, height) / 2 * kTerrainMarginFactor`,
    so the water always covers the largest terrain dimension and protrudes
    beyond it on every side.
  - `grid_size` is recomputed from the new world size so cell density stays
    constant (`kCellSize = 10 wu/cell`).

### `src/editor/EnvironmentEditorPanel.cpp`

`EnableWater()` iterates `scene_->GetObjects()` to find a `GameTerrain`, reads
its `TerrainData::GetWorldWidth()` / `GetWorldHeight()`, and forwards them to
`WaterRenderer::Build()`. If no terrain is in the scene the old default
behaviour is kept.

### `src/app/main.cpp`

Water is built after a pre-scan of `map_data.objects` that locates the
`GameTerrain` (if any) and extracts terrain dimensions before `WaterRenderer`
is constructed. This avoids reordering the full object-loading loop.

## Additional fix: uint32 index buffer

The recomputed `grid_size` for a large terrain (e.g. 4096 texels × 2 m/texel =
8192 m world) becomes 984, giving 985² = 970 225 vertices — well above the
uint16 limit of 65 535. The original uint16 index buffer silently wrapped,
producing corrupted geometry near the origin. Switched to `kUInt32` throughout
`BuildMesh()`.

## `EnvironmentEditorPanel`: terrain lookup via `GetType()` not `dynamic_cast`

Replaced `dynamic_cast<game::GameTerrain*>` with
`obj->GetType() == game::GameObjectType::kTerrain` + `static_cast`, matching
the pattern already used in `EditorWindow::FindTerrain()`.

## Decisions

- **Margin factor 1.2** (20 % extension): large enough to hide the terrain edge
  under most camera angles, small enough to keep the vertex count reasonable.
- **Square water grid**: kept square for simplicity; the half-extent uses
  `max(width, height)` so rectangular terrains are still fully covered.
- **No rebuild on terrain resize**: if the terrain is resized post-load the
  water renderer keeps the dimensions from construction time. This is acceptable
  because both water and terrain require a map reload to change dimensions.
- **Terrain origin is (0, 0)**: the CDLODQuadTree places all patches with
  `patch_origin` starting at (0, 0), so centering the water at
  `(world_width/2, world_height/2)` is correct for all maps.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per file pair, Google C++ style, include root is
  `src/`.
- `environment/CLAUDE.md`: no dependency on `renderer`; `editor` → `environment`
  direction respected.
- `editor/CLAUDE.md`: editor is the leaf; `EnvironmentEditorPanel` already
  depended on `game` and `terrain` indirectly through `EditorScene`.
- `cppcheck-suppress unusedStructMember` added to new member variables as per
  project convention.
