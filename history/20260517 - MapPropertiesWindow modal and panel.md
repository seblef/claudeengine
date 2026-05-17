# MapPropertiesWindow — modal (New) and panel (Map Properties)

**Date:** 2026-05-17
**Issue:** #253
**Branch:** feat/map-properties-window-253

---

## Summary

Added `MapPropertiesWindow`, a dual-mode ImGui widget that handles map-level
properties in two contexts:

- **Panel mode** (`"Map Properties"` dockable panel): edits the live scene in
  real time via `EditorScene::SetMapName()` and `EditorScene::SetGlobalLightDesc()`.
  Map size is permanently read-only (fixed at creation).
- **Modal mode** (`"New Map##modal"` popup): scratch-state editing; returns
  `true` from `RenderModal()` exactly once when the user clicks "Create".
  `EditorWindow` then reconstructs the scene with the entered parameters.

---

## Files changed

| File | Change |
|---|---|
| `src/editor/MapPropertiesWindow.h` | New header |
| `src/editor/MapPropertiesWindow.cpp` | New implementation |
| `src/editor/EditorWindow.h` | Added `map_properties_`, `new_map_pending_` members; forward-declared `MapPropertiesWindow` |
| `src/editor/EditorWindow.cpp` | Wired panel + modal rendering; added `File > New` menu item |
| `src/editor/CMakeLists.txt` | Added `MapPropertiesWindow.cpp` |

---

## Decisions and rationale

### Single class, two render paths

The issue specified both modes on one class. This keeps light-field rendering
(`RenderLightFields`) a single implementation shared by both modes, avoiding
duplication.

### `scene_` reconstruction on "Create"

`File > New` destroys the old scene and creates a fresh `EditorScene` with the
entered name, size, and light. The `MapPropertiesWindow` and `EditorViewport`
scene pointer are both updated at that point. This is safe because all scene
consumers in `EditorWindow` receive their pointer via `SetScene()` / constructor
and hold raw non-owning pointers.

### `new_map_pending_` flag

`ImGui::OpenPopup()` must be called **outside** any `Begin/End` pair (between
frames, not from inside a menu callback). A one-frame boolean flag bridges
the menu callback into the correct callsite in `Render()`.

### Captured values before scene reset

`map_properties_->GetNewMap*()` accessors are read into local variables before
`map_properties_` is replaced, avoiding a use-after-move bug.

### Map size read-only in panel mode

The issue mandates this: map dimensions are fixed at creation and cannot be
changed at runtime without rebuilding the floor plane. `ImGui::BeginDisabled()`
greys the field visually.

---

## Fields rendered

| Field | Widget | Mode |
|---|---|---|
| Map name | `InputText` | both (live in panel, scratch in modal) |
| Filename | `LabelText` (read-only) | both |
| Map size | `InputFloat` | modal only (disabled in panel) |
| Light direction | `DragFloat3` step 0.01, normalized | both |
| Light color | `ColorEdit3` | both |
| Ambient color | `ColorEdit3` | both |
| Intensity | `DragFloat` 0.01, [0, 10] | both |
| Cast shadow | `Checkbox` | both |
| Shadow resolution | `InputInt` | both |
| Shadow bias | `DragFloat` 0.0001, [0, 0.1], "%.4f" | both |

---

## Output to keep in mind

- `MapPropertiesWindow` has no file-I/O dependency — serialization is handled
  by `MapSerializer`.
- When "Create" is confirmed, `EditorWindow` reconstructs `scene_` and
  `map_properties_`; this resets undo history implicitly (history is a member
  of `EditorWindow` and is not cleared — this may need to be addressed in a
  future issue).
- The `"Map Properties"` panel is always rendered; no open/close toggle exists
  yet (same pattern as other panels).

---

## Skills and instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md`: one class per file, ImGui calls inside `NewFrame/Render`,
  subsystems owned by `EditorSystem`/`EditorWindow`
- `src/CLAUDE.md`: Google C++ style, include root is `src/`, no cross-layer imports
- Conventional commits format for the commit message
