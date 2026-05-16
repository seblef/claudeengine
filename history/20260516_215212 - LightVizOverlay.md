# Light Wireframe Overlay in EditorViewport

## Summary

Implemented GitHub issue #216: visual representation of all `GameLight` objects in the
editor viewport. For every light in the scene, a wireframe geometry is projected onto
the ImGui draw list overlay — drawn after the selected-object bounding box — giving
lights a visual presence without any additional renderer passes.

## Changes

### `src/editor/EditorViewport.h`
- Added `DrawLightsOverlay(ImDrawList*, ImVec2, ImVec2) const` private method declaration.

### `src/editor/EditorViewport.cpp`
- Added includes: `<cmath>`, `core/Vec3f.h`, `game/GameLight.h`,
  `renderer/CircleSpotLight.h`, `renderer/GlobalLight.h`, `renderer/Light.h`,
  `renderer/OmniLight.h`, `renderer/RectangleSpotLight.h`.
- Added `DrawLineSegment3D()` anonymous-namespace helper: projects a 3D segment
  through the VP matrix and draws it; skips segments with either endpoint behind
  the camera (`clip.w <= 0`).
- Added `DrawCircle3D()` anonymous-namespace helper: approximates a circle as a
  32-segment polyline in an arbitrary world-space plane defined by two orthogonal
  axis vectors; calls `DrawLineSegment3D` for each segment.
- Added `DrawLightWireframe()` anonymous-namespace function: dispatches on
  `renderer::LightType` and draws type-appropriate geometry:
  - **OmniLight**: 3 circles in the XY, XZ, YZ planes centred at world position.
  - **CircleSpotLight**: apex → 4 rim lines (0°/90°/180°/270°) + base circle.
  - **RectangleSpotLight**: apex → 4 corner edges + base rectangle perimeter.
  - **GlobalLight**: shaft + 2-line arrowhead from world origin along direction.
- Implemented `EditorViewport::DrawLightsOverlay()`: iterates `EditorScene::GetObjects()`,
  filters `GameObjectType::kLight`, casts to `GameLight*`, extracts world position
  from column 3 of the world transform, and calls `DrawLightWireframe` with the
  appropriate color (yellow 0xFFFFCC00 unselected, orange 0xFF0088FF selected).
- Added `DrawLightsOverlay` call in `EditorViewport::Render()` after the
  selected-object bbox drawing.

## Decisions and rationale

### Static casts vs dynamic_cast
Static casts are used after switching on `light.GetType()`, which guarantees the
concrete type. This avoids RTTI overhead and keeps the hot draw path lean.

### World position extraction
`wt(0,3)`, `wt(1,3)`, `wt(2,3)` read the translation column of the row-major
`Mat4f`. Verified against the `Mat4f::Translation` factory which places `t.x/y/z`
at those positions.

### Drawing before ImGuizmo setup
`DrawLightsOverlay` is called immediately after `DrawSelectedBBox`, before
`ImGuizmo::SetDrawlist()`, so it writes to the ImGui window draw list via
`ImGui::GetWindowDrawList()`, consistent with `DrawSelectedBBox`.

### GlobalLight origin anchor
The arrow is drawn from `Vec3f::kZero` (world origin) rather than the light's
world position, which for a `GlobalLight` is always the identity. This matches
the directional-light concept (infinite distance, direction only).

## Key things to remember

- `Mat4f` is row-major; translation lives at `(row, col) = (0,3), (1,3), (2,3)`.
- VP matrix convention: `clip = Vec4f(p, 1) * vp` (row-vector, consistent with
  the existing `DrawBBoxWireframe`).
- ImU32 color is ABGR in ImGui: `0xFFFFCC00` = yellow, `0xFF0088FF` = orange.
- `DrawCircle3D` uses `kAxisY` / `kAxisX` fallback for the "up" axis when the
  light direction is nearly parallel to Y — identical pattern to `LookAt`
  implementations.

## Skills and instructions followed

- Read all relevant files before writing any code (issue instruction).
- One class per `.h`/`.cpp` pair (`src/CLAUDE.md`).
- Google C++ style, meaningful doc comments (`src/CLAUDE.md`).
- `cpplint` clean (`CLAUDE.md` git workflow step 6).
- Conventional commit message (`CLAUDE.md` git workflow step 7).
- Feature branch `feat/light-viz-overlay` from `dev` (`CLAUDE.md` / issue).
