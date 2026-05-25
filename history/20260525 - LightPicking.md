# Light Picking and GlobalLight Overlay Removal (#270)

## Summary

Two related changes to light handling in `EditorViewport`:

1. **GlobalLight removed from the scene overlay** — the global light is a map-level property (configured in Map Properties), not a scene object that should be visually annotated or selectable.
2. **Screen-space proximity picking for positional lights** (Omni, CircleSpot, RectSpot) implemented using the `PickingAccelerator` as a pre-filter.

## Changes

### `src/editor/EditorViewport.cpp`

#### `DrawLightWireframe` — GlobalLight case removed
The `kGlobal` switch case now does nothing (empty `break`). The `#include "renderer/GlobalLight.h"` was also removed as it is no longer needed.

#### `DrawLightsOverlay` — GlobalLight skipped
Added an early `continue` when `light->GetType() == renderer::LightType::kGlobal`, so the global light no longer gets a wireframe overlay.

#### New anonymous-namespace helpers

| Function | Purpose |
|---|---|
| `ScreenDistToSegment` | Minimum 2D pixel distance from a point to a screen-space segment |
| `ProjectToScreen` | Projects a world-space `Vec3f` via the VP matrix to screen pixels; returns `valid=false` if behind camera |
| `LightPickDistPx` | Mirrors `DrawLightWireframe` geometry to compute the minimum screen-space distance (pixels) between the cursor and a light's wireframe |

`LightPickDistPx` uses two local lambdas (`seg_dist`, `circle_dist`) to avoid repeating the project-and-measure pattern for each segment and circle arc.

#### `PickObjectAt` — light pass added

After the existing mesh pass, a second loop over `candidates` (same `PickingAccelerator` result) handles lights:

- Skips non-light objects and GlobalLight.
- Calls `LightPickDistPx` to get the cursor-to-wireframe distance in pixels.
- Tracks the closest light within `kLightPickThresholdPx = 8 px`.

**Mesh vs light priority**: if both a mesh hit and a light hit exist, the winner is whichever is closer to the camera — the mesh hit uses ray-parameter `t_best`, the light hit uses the Euclidean distance from the ray origin to the light's world position.

## Decisions and rationale

- **Threshold of 8 px** matches common "fat finger" tolerance for clicking on 1-pixel-thin wireframes.
- **Depth comparison** uses `t_best` (ray depth for meshes) vs. `(light_pos - ray_origin).Length()` (world distance for lights). For the typical case of uniform-scale meshes and positional lights this is a valid apples-to-apples comparison.
- **`PickingAccelerator` as pre-filter**: light candidates come from the same `QueryRay` result already used for meshes, keeping the two passes consistent and avoiding an extra full-scene scan.
- **No new header files**: all helpers are `static`-linkage free functions in the anonymous namespace of `EditorViewport.cpp`, consistent with the existing `DrawLightWireframe` / `DrawBBoxWireframe` pattern.

## Output to keep in mind

- `LightPickDistPx` deliberately mirrors `DrawLightWireframe`'s geometry. If the wireframe geometry ever changes (e.g. circle segment count, spot light cone lines), both functions must be updated together.
- `ScreenPt` struct is now defined twice in the anonymous namespace (once inline in `DrawBBoxWireframe` and once as a free struct). This is harmless because both are in the same TU anonymous namespace — if the file grows further, consider hoisting to a single definition.

## Skills and instructions followed

- `src/CLAUDE.md`: Google C++ style, no new class per function, include root is `src/`.
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; no new singletons.
- Root `CLAUDE.md`: conventional commits, `cpplint` before commit, history file required.
