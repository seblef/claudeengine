# MaterialPropertyCommand — Undo/redo for material property changes

**Date:** 2026-05-17
**Branch:** feat/material-property-command-239
**Issue:** #239

---

## Summary

Makes every material property edit in `MaterialEditorWindow` undoable via the
shared `EditorCommandHistory`. Follows the same snapshot approach already used
by `LightPropertyCommand`.

---

## New files

### `src/editor/commands/MaterialPropertyCommand.h` / `.cpp`

**`MaterialSnapshot`** captures the full editable state of a
`game::GameMaterial`:

| Field | Type | Source |
|---|---|---|
| `diffuse_color` | `core::Color` | `Material::GetDiffuseColor()` |
| `emissive_color` | `core::Color` | `Material::GetEmissiveColor()` |
| `ambient_color` | `core::Color` | `Material::GetAmbientColor()` |
| `shininess` | `float` | `Material::GetShininess()` |
| `texture_slots` | `std::vector<std::string>` | `Texture::GetId()` per slot |

`CaptureSnapshot(game_mat)` reads all five fields + texture IDs into a value
object. `ApplySnapshot(game_mat, s, video)` writes them back, recreating
`abstract::Texture` objects from their stored IDs via `VideoDevice::CreateTexture`.

**`MaterialPropertyCommand`** implements `EditorCommand` with `Execute()`,
`Undo()`, `Redo()`:
- `Execute()` / `Redo()` → `ApplySnapshot(after_)`
- `Undo()` → `ApplySnapshot(before_)` + `LOG_F(INFO, ...)` warning that the
  saved YAML file is unaffected

The command holds a `abstract::VideoDevice*` because texture restoration
requires re-creating GPU texture objects from their IDs.

---

## Changes to existing files

### `MaterialEditorWindow.h`

- Added `#include "editor/commands/MaterialPropertyCommand.h"` (brings in
  `MaterialSnapshot` type for the `before_snapshot_` member).
- Added `SetCommandHistory(EditorCommandHistory*)` setter (same pattern as
  `PropertiesPanel`).
- Added `history_` and `before_snapshot_` members.

### `MaterialEditorWindow.cpp`

- Added `track()` lambda in `RenderRenderingSection()` — called after each
  `ColorEdit4` and `SliderFloat` widget, mirrors the `PropertiesPanel` pattern:
  captures `before_snapshot_` on `IsItemActivated()`, pushes a
  `MaterialPropertyCommand` on `IsItemDeactivatedAfterEdit()` if state changed.
- `RenderTextureSlot()`: texture-select (NFD dialog) and clear ("x") buttons
  capture a before snapshot immediately before the change and push a command
  after, since these are immediate (non-drag) operations.

### `EditorWindow.cpp`

- Added `material_editor_->SetCommandHistory(&history_)` in the constructor,
  alongside the existing wiring for `properties_panel_`, `viewport_`, and
  `toolbar_`.

### `src/editor/commands/CMakeLists.txt`

- Added `MaterialPropertyCommand.cpp` to the `editor` target sources.

---

## Decisions and rationale

### Why store texture IDs rather than raw pointers?

`abstract::Texture` is ref-counted GPU state. Storing raw pointers in a
snapshot would require keeping the object alive across undo/redo, complicating
lifetime management. Storing IDs is a thin string copy; recreating the texture
via `VideoDevice::CreateTexture` is cheap because the video device caches
textures by ID.

### Why always log the YAML-unchanged warning on every Undo?

The issue allows "tooltip or log warning". Detecting "the user saved *after*
this command" would require threading a flag through the command, coupling it
to `MaterialEditorWindow` internals. Logging on every undo is simpler, always
correct, and harmless in practice (the user will rarely undo material changes
frequently enough for the log to be noise).

### Texture slot `operator==` via `std::vector<std::string>` comparison

`MaterialSnapshot::operator==` uses `std::vector<std::string>::operator==`
which compares element-by-element. This is correct and avoids custom comparison
logic.

---

## Output to keep in mind for future features

- `MaterialPropertyCommand` takes `abstract::VideoDevice*` — future commands
  that restore GPU resources (e.g., mesh geometry) should follow the same
  pattern.
- `MaterialEditorWindow` now follows the same history-wiring convention as
  `PropertiesPanel`: `SetCommandHistory()` + `before_snapshot_` member.
- The `track()` lambda pattern (`IsItemActivated` / `IsItemDeactivatedAfterEdit`)
  is now established for both `PropertiesPanel` and `MaterialEditorWindow` and
  should be reused for any future property editor that adds undoable widgets.

---

## Skills and instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md`: one class per `.h/.cpp` pair; editor is the leaf module
- `src/CLAUDE.md`: Google C++ style, include root is `src/`, cpplint required
- Root `CLAUDE.md`: conventional commits, history markdown, git workflow
