# Snap Settings — Toolbar Toggle and Config Persistence

**Issue**: #809  
**PR**: #814  
**Branch**: `feat/snap-settings-toolbar-persistence`  
**Date**: 2026-06-26

## What was done

Added a snap-state foundation to the editor toolbar (issue #809, the first issue
in the "Editor: Object Snapping System" milestone):

### EditorToolbar (`src/editor/EditorToolbar.h/.cpp`)

- Added four private fields: `snap_enabled_` (bool, default `false`),
  `position_snap_` (float, default `0.1`), `rotation_snap_` (float, default
  `45.0`), `scale_snap_` (float, default `0.1`), each guarded by
  `cppcheck-suppress unusedStructMember`.
- Added `SetOnSnapChanged(std::function<void()>)` callback, fired whenever the
  toggle or a DragFloat changes.
- Added `SetSnapEnabled / SetPositionSnap / SetRotationSnap / SetScaleSnap`
  setters (called by `EditorWindow` at load time, do **not** fire the callback).
- Added `IsSnapEnabled / GetPositionSnap / GetRotationSnap / GetScaleSnap`
  getters (public API for downstream tools).
- Rendered in `Render()` after the sound toggle: a separator, a magnet button
  (`ICON_FA_MAGNET`, teal when active, same pattern as the sound toggle), then
  three inline `DragFloat` widgets (pos 55 px, rot 50 px, scl 55 px). Each
  DragFloat clamps its own minimum via `std::max` to prevent zero/negative values.
- Added `#include <algorithm>` for `std::max`.

### EditorWindow (`src/editor/EditorWindow.h/.cpp`)

- Declared `SaveSnapSettings()` in the header.
- Wired `toolbar_->SetOnSnapChanged([this]{ SaveSnapSettings(); })` in the
  constructor alongside other toolbar callbacks.
- In the constructor config-loading block (after overlay settings): reads
  `editor.snap.{enabled,position,rotation,scale}` from `config.yaml` and applies
  them to the toolbar via the setters.
- `SaveSnapSettings()` follows the exact pattern of `SavePhysicsDebugSettings()`
  and `SaveOverlaySettings()`: loads the YAML root, sets the four nodes, writes
  back.
- Added `SaveSnapSettings()` call in `~EditorWindow()` alongside the two
  existing save calls.

### data/config.yaml

Not committed (gitignored). The `editor.snap` section is created on first
`SaveSnapSettings()` call. Default values documented in the issue spec:
```yaml
editor:
  snap:
    enabled: false
    position: 0.1
    rotation: 45.0
    scale: 0.1
```

## Decisions and rationale

| Decision | Rationale |
|---|---|
| DragFloat controls always visible (not hidden when snap is off) | Values are editable even when snap is off, matching common editor patterns; less surprising than controls appearing/disappearing |
| `on_snap_changed_` callback fires on every change | Saves immediately; consistent with how `on_sound_toggle_` triggers immediate side-effects |
| Setters do NOT fire the callback | Prevents a spurious disk write during startup load |
| `std::max` clamp inside `Render()` | Simpler than ImGui's `v_min` alone since DragFloat min is a soft limit when dragging |
| No keyboard shortcut for snap toggle | Issue spec explicitly says "no default shortcut" |

## Output to keep in mind for next issues

- All other snapping issues (#810+) depend on this. They can call
  `toolbar_->IsSnapEnabled()` / `Get*Snap()` via the `EditorToolbar` handle
  already held by `EditorWindow`.
- The `TransformTool` and `PlacementTool` receive the toolbar pointer indirectly
  through `EditorWindow`; a clean approach for those tools is to accept a raw
  `const EditorToolbar*` and query it each frame.
- `data/config.yaml` is gitignored — do not attempt to commit it.

## Skills and CLAUDE.md instructions used

- `/impl-issue` skill
- `src/editor/CLAUDE.md` — `cppcheck-suppress unusedStructMember` pattern,
  one class per `.h`/`.cpp` rule, GUI vs edition logic separation note
- Global `CLAUDE.md` — conventional commits, history file requirement, cpplint
  step, PR to `dev` not `main`
