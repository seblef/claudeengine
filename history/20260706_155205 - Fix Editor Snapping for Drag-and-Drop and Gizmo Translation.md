# Fix Editor Snapping for Drag-and-Drop and Gizmo Translation

**Issue**: #823
**Branch**: `feat/editor-snapping-fix`
**Date**: 2026-07-06

## What changed

Fixed two independent snapping bugs reported in #823.

### Bug 1 — Drag-and-drop mesh placement ignored snap settings

`EditorViewport::PlaceMeshAt()` placed the dropped mesh at the raw terrain-hit
position without ever consulting the toolbar's snap state, unlike
`PlacementTool::UpdatePreviewPosition()` which already applied
`toolbar_->IsSnapEffectivelyEnabled()` / `GetPositionSnap()` / `SnapValue()`
correctly for all other creation tools.

Fix: `PlaceMeshAt()` now snaps `hit.x` / `hit.z` the same way before building
the translation matrix, when snapping is enabled on the toolbar. `y` is left
untouched (it comes from the terrain hit, same as `PlacementTool`).

### Bug 2 — Gizmo translation snapped the delta, not the absolute position

ImGuizmo's native snap (`ComputeSnap()` in `HandleTranslation()`) rounds the
mouse-driven delta relative to the drag-start position (`mMatrixOrigin`), not
the destination position. Algebraically: `final = original + round(delta /
snap) * snap` instead of the expected `final = round((original + delta) /
snap) * snap`. This is vendored third-party code
(`external/imguizmo-src/src/ImGuizmo.cpp`), so it was left untouched rather
than patched, to avoid diverging from upstream on every library update.

Fix applied in `TransformTool` instead:
- `BuildSnapArray()` now returns `nullptr` for `ImGuizmo::TRANSLATE`
  regardless of toolbar state — ImGuizmo's own snap is never used for
  translation any more (still used as before for `ROTATE` / `SCALE`, which
  don't have this absolute-vs-relative problem since angle/scale-factor
  deltas are what's meant to be snapped there).
- A new private `GetActivePositionSnap()` returns the position-snap step
  when `op_ == TRANSLATE` and snapping is enabled, `0.f` otherwise.
- A new anonymous-namespace `SnapTranslation(core::Mat4f*, float)` rounds a
  transform's translation column (all 3 axes) to the nearest multiple of the
  snap step, in place, in our engine's column-vector `Mat4f` convention.
- Single-object path: after `ImGuizmo::Manipulate()` returns, the resulting
  transform's translation is snapped before calling `obj->SetWorldTransform()`.
- Multi-object path: the pivot transform (bbox-centre gizmo) is snapped the
  same way before being used to derive each object's new transform
  (`new_T[i] = pivot_after * T(-centre_init) * T_before[i]`), and the
  *snapped* pivot is what's stored into `pivot_im_` for the next frame, so
  the gizmo widget visually tracks the snapped position instead of drifting
  from it.

### Files modified

| File | Change |
|------|--------|
| `src/editor/EditorViewport.cpp` | `PlaceMeshAt()` snaps `x`/`z` via `toolbar_`/`SnapValue()`; added `#include "editor/EditorUtils.h"` |
| `src/editor/tools/TransformTool.h` | Added private `GetActivePositionSnap()`, updated `BuildSnapArray()` doc comment |
| `src/editor/tools/TransformTool.cpp` | `BuildSnapArray()` excludes `TRANSLATE`; added `GetActivePositionSnap()` and anonymous-namespace `SnapTranslation()`; both gizmo paths snap the absolute translation post-`Manipulate()` |

## Design decisions

### Post-process in `TransformTool` rather than patch vendored ImGuizmo

The issue offered both options. Patching `external/imguizmo-src` would fix the
bug at its root but creates a maintenance burden: the patch has to be
manually reapplied (or the vendoring re-forked) every time ImGuizmo is
updated, and nothing in the build enforces that. Post-processing in
`TransformTool` keeps the fix in engine-owned code, colocated with the
existing (correct) snap logic in `PlacementTool`, and is a small, local change.

### Only `TRANSLATE` is rerouted, not `ROTATE`/`SCALE`

The delta-vs-absolute distinction is specific to *position*: an absolute
world position has a natural "grid" to snap to, so drag-start-relative
snapping visibly disagrees with it once the object doesn't start on a grid
line. Rotation and scale snapping are inherently about the *amount* changed
during the drag (snap the applied rotation angle / scale factor to 15°
steps, etc.), so ImGuizmo's native delta-based snap is the semantically
correct behavior there and was left untouched.

### Snapping the pivot (not each object individually) in the multi-select path

For multi-selection, all dragged objects move by the same rigid delta
relative to the pivot (bbox centre). Snapping each object's own position
independently would make objects lose their relative spacing (only correct
if every object already sat on a grid line). Snapping the *pivot* instead
preserves relative offsets between objects — consistent with how rotation/
scale already pivot around the group centre in this same tool.

## Output to keep in mind

- `SnapValue()` (in `editor/EditorUtils.h`) is now used from three places:
  `PlacementTool`, `EditorViewport::PlaceMeshAt()`, and
  `TransformTool::SnapTranslation()`. If a fourth snapping call site appears,
  consider whether `SnapTranslation()`-style helpers belong in `EditorUtils.h`
  too instead of being duplicated per-tool.
- `TransformTool::GetActivePositionSnap()` deliberately returns `0.f` (not
  `std::optional`) as a "no snap" sentinel, matching `SnapTranslation()`'s
  `snap <= 0.f` early-out — mirrors the existing `step <= 0.f` guard pattern
  in `EditorViewport::DrawSnapGrid()`.
- Not verified interactively in a running editor session in this
  contribution: this sandbox has no headless X server (no `xvfb-run`/`Xvfb`)
  and no input-automation tool (no `xdotool`/`ydotool`) to script a gizmo
  mouse-drag, and the only available display is the user's real desktop.
  The user opted to test manually (drag-and-drop placement with snap on, and
  gizmo-drag an object starting at a non-grid-aligned position) rather than
  have this attempted headlessly. Build (`wreckoning_editor`, `wreckoning`)
  succeeds, `cpplint` is clean, and all existing unit tests
  (`core_tests`, `game_tests`, `mesh_tests`) pass unchanged.

## Skills and instructions applied

- `impl-issue` skill (branch from dev, cpplint, conventional commit, PR to dev)
- `verify` skill: attempted to find a runtime surface to drive; concluded
  BLOCKED for interactive GUI verification (no headless display/input tooling)
  and asked the user how to proceed rather than silently skipping or hijacking
  their real desktop session.
- `src/CLAUDE.md`: Google C++ style, include paths relative to `src/`
- `src/editor/CLAUDE.md`: one class per file, GUI/logic separation (fix lives
  in tool/viewport logic, not in any `*Panel`/`*Window`)
