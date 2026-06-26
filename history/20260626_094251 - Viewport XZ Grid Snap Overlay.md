# Viewport XZ Grid Snap Overlay

**Issue**: #812  
**PR**: #817  
**Branch**: `feat/viewport-xz-grid-snap`

## What was built

A world-space XZ grid overlay rendered in the editor viewport whenever snapping is enabled. The grid gives immediate spatial context for the active snap increment.

## Changes

### `src/editor/EditorToolbar.h`
- Added `show_grid_` bool member (default `true`) with `SetShowGrid(bool)` / `IsShowGrid()` accessors.
- No toolbar UI: this is a config-only knob that lets users decouple grid visibility from snap state by setting `editor.snap.show_grid: false` in `config.yaml`.

### `src/editor/EditorViewport.h`
- Forward-declared `EditorToolbar`.
- Added `SetToolbar(const EditorToolbar*)` public method and `toolbar_` non-owning member.
- Added private `DrawSnapGrid(ImVec2 image_pos, ImVec2 image_size)`.

### `src/editor/EditorViewport.cpp`
- Added `#include "editor/EditorToolbar.h"`, `#include "core/Vec4f.h"`, `#include <cmath>`.
- `DrawSnapGrid()` implementation:
  - Guards: returns immediately if toolbar absent, snap disabled, `show_grid` false, or `step <= 0`.
  - Snaps the grid origin to `floor(focus / step) * step` on X and Z so the grid jumps by one cell as the camera moves rather than scrolling continuously.
  - Draws 21 lines per axis (±10 cells, `kHalfCells = 10`), for a 20×20 grid.
  - Major lines every `kMajorEvery = 10` cells are `IM_COL32(128,128,128,128)`; minor lines are `IM_COL32(102,102,102,89)`.
  - World→screen projection uses the VP matrix directly (`Vec4f * vp`, same pattern as `PickingUtils.cpp`); lines where either endpoint is behind the camera are skipped.
  - Draws on `ImGui::GetWindowDrawList()`.
- `DrawSnapGrid()` called in `Render()` inside the `!in_play_mode_` block, after `WireframeRenderer::Render()` and before the ImGuizmo orbit widget.

### `src/editor/EditorWindow.cpp`
- Added `viewport_->SetToolbar(toolbar_.get())` after other viewport wiring.
- `LoadConfig()`: reads `editor.snap.show_grid` → `toolbar_->SetShowGrid()`.
- `SaveSnapSettings()`: persists `editor.snap.show_grid` alongside other snap values.

## Decisions

- **No UI toggle for `show_grid`**: The issue called it an optional enhancement. Since there's no design for where to put the button and the toolbar is already crowded, config.yaml-only was the simplest correct implementation. Can be promoted to a checkbox in the snap row of the toolbar in a follow-up.
- **`EditorViewport` holds `const EditorToolbar*`**: Follows the same `SetToolbar`/`toolbar_` pattern already used by `TransformTool` and `PlacementTool`. Non-owning, set at startup.
- **Skip both-behind-camera lines**: Same clipping policy as `DrawBBoxWireframe` in `PickingUtils.cpp`. For a horizon-skimming camera some lines at the far edges may disappear, but this is acceptable and consistent with the rest of the overlay system.
- **Y = 0 plane**: The grid is on the XZ ground plane (Y=0), which matches the default terrain and is the natural reference for object placement snap.

## Things to keep in mind for follow-up features

- If the terrain is not at Y=0, the grid will be visually disconnected from the surface. A future improvement could sample the terrain at the focus point and draw at that Y.
- `show_grid` could be promoted to a toolbar toggle (e.g., a small grid icon next to the snap button) when the toolbar design allows it.
- The `kHalfCells = 10` and `kMajorEvery = 10` constants in `DrawSnapGrid()` are in the `.cpp` and easy to tune.

## Skills and instructions used

- **`impl-issue` skill** — workflow (checkout dev, branch, implement, cpplint, commit, PR).
- **`src/editor/CLAUDE.md`** — one class per file, no editor singletons, `cppcheck-suppress unusedStructMember` for all non-trivial private members, `std::find_if` over raw loops.
- **`src/CLAUDE.md`** — Google C++ style, include root is `src/`, no multi-line comments.
- **Existing overlay pattern** (`PickingUtils.cpp`) — reused VP-matrix world→screen projection formula verbatim for consistency.
