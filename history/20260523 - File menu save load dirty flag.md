# File menu (New/Load/Save/Save As), Ctrl+S, toolbar Save, dirty flag

**Date:** 2026-05-23
**Issue:** #254
**Branch:** feat/file-menu-save-load-254
**Milestone:** Support for maps loading & saving

---

## Summary

Completes milestone 10 by wiring map persistence into the editor UI. The user can now create, load, save, and save-as maps from a **File** menu, via Ctrl+N/Ctrl+S keyboard shortcuts, and via a **Save** toolbar button. A dirty flag tracks unsaved edits and gates the Save button and warns the user before destructive actions (New / Load).

---

## Changes

### `EditorCommandHistory` — dirty callback

Added `SetOnDirty(std::function<void()>)` and fired it at the end of `Push()`. `EditorWindow` wires a `[this]{ scene_dirty_ = true; }` lambda in its constructor, so any undoable command automatically marks the scene dirty.

### `MapPropertiesWindow` — change detection

Changed `RenderPanel()` return type from `void` to `bool` (returns true if any field was edited). Changed `RenderLightFields()` similarly: now returns `bool` and collects return values from all ImGui widgets (previously the last four widgets — Intensity, Cast shadow, Shadow resolution, Shadow bias — silently ignored their return values). `RenderPanel()` only calls `SetGlobalLightDesc()` when `RenderLightFields()` returns true, avoiding unnecessary renderer syncs every frame.

### `EditorToolbar` — Save button

Added:
- `SetOnSave(std::function<void()> cb)` — callback fired on click.
- `SetDirty(bool dirty)` — controls `ImGui::BeginDisabled` on the Save button.
- A `ICON_FA_FLOPPY_DISK` button with tooltip "Save (Ctrl+S)", placed after the Undo/Redo section and before the separator that leads into the transform tools.

### `EditorWindow` — full file operations

**New members:**
- `bool scene_dirty_` — true when the scene has unsaved edits.
- `bool show_map_props_` — toggles the Map Properties panel (default false).
- `bool open_unsaved_changes_modal_` — triggers `OpenPopup` outside any Begin/End pair.
- `std::function<void()> pending_after_save_` — action deferred until the unsaved-changes modal is resolved.

**New methods:**
- `SaveCurrent()` — saves to `scene_->GetFilePath()`; falls through to `SaveAs()` when the path is empty.
- `SaveAs()` — opens an NFD save dialog defaulting to `<mapname>.map.yaml`, appends the extension if missing, calls `MapSerializer::Save`, marks clean on success.
- `LoadFromFile()` — opens an NFD open dialog, calls `MapSerializer::Load`, swaps `scene_` and `map_properties_`, restores camera state, clears history, marks clean.
- `CheckDirtyThenRun(on_proceed)` — fires `on_proceed` directly if clean; otherwise stores it in `pending_after_save_` and sets `open_unsaved_changes_modal_`.
- `RenderUnsavedChangesModal()` — renders a three-button modal (Save / Discard / Cancel); fires the stored callback on Save or Discard.

**File menu** (replaces the previous stub):
```
File
├── New              Ctrl+N
├── Load...
├── ───────────────────────
├── Save             Ctrl+S
├── Save As...
└── ───────────────────────
    Quit
```

**Map menu** (new):
```
Map
└── Map Properties   (checkmark toggles show_map_props_)
```

**Map Properties panel** is now conditional on `show_map_props_` and passes `&show_map_props_` to `ImGui::Begin()` so the X close button works.

**Status bar** now shows "● Unsaved changes" when `scene_dirty_` is true.

**Keyboard shortcuts:** `Ctrl+S` → `SaveCurrent()`, `Ctrl+N` → `CheckDirtyThenRun([this]{ new_map_pending_ = true; })`.

**Toolbar wiring:** `toolbar_->SetOnSave([this]{ SaveCurrent(); })` and `toolbar_->SetDirty(scene_dirty_)` called each frame before `toolbar_->Render()`.

---

## Decisions

- **`open_unsaved_changes_modal_` flag pattern** (same as existing `new_map_pending_`): ImGui requires `OpenPopup` to be called outside `BeginPopupModal`. Rather than a separate deferred-action queue, a simple bool flag is enough.
- **`show_map_props_ = false` by default**: the Map Properties panel is opt-in via the Map menu, consistent with most docking-based editors.
- **Extension check in `SaveAs()`**: `path.extension() != ".yaml" || path.stem().extension() != ".map"` guards against the user typing a filename without the `.map.yaml` compound extension.
- **`LoadFromFile()` is called directly from `pending_after_save_`**: NFD dialogs are blocking native dialogs; calling them from inside an ImGui frame is safe (same as `ImportMaterial`/`ImportMesh`).

---

## Skills and instructions used

- `impl-issue` skill
- CLAUDE.md: Google C++ style guide, one class per file, include root is `src/`
- editor/CLAUDE.md: nfd CMake target, ImGui lifecycle constraint
- Conventional commits, PR to `dev`, history markdown

---

## Output to keep in mind

- `MapPropertiesWindow::RenderLightFields()` now correctly returns `bool` — if any future panel uses it (e.g. a "Create light" dialog), it can propagate change detection.
- The dirty flag is currently cleared only by Save, Save As, New, and Load. Undo/Redo do **not** clear it (conservative: even after undoing all changes, the flag stays set). This could be improved with a "generation counter" approach if needed later.
- The status bar "● Unsaved changes" text is placeholder UI; a future task could add more detailed information (filename, last save time).
