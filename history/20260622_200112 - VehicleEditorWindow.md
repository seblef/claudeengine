# Vehicle Editor — dedicated floating window (issue #740)

## Context

Follow-up to #717/#739. `VehiclePanel` lived as a tab inside the
`ResourceBrowser` panel via the `IResourcePanel` / `ResourcePanelRegistry`
mechanism. Issue #740 promotes it to a dedicated, freely resizable floating
window — consistent with `MaterialEditorWindow`, `MeshEditorWindow`, and
`ParticleEditorWindow`.

## What changed

### New: `VehicleEditorWindow` (`editor/VehicleEditorWindow.h/.cpp`)

Replaces `VehiclePanel`. All internal logic (YAML load/save, mesh picking,
combined preview, ImGuizmo wheel gizmo, physics sliders) is preserved verbatim.
Key differences from `VehiclePanel`:

* Implements no interface — it is a self-contained window, not an
  `IResourcePanel` tab.
* `Open(path)` re-opens the window (or re-focuses it) for a new path; it resets
  all state and calls `LoadFromYaml`.
* `Render()` creates a top-level ImGui window every frame and is a no-op when
  `show_` is false.
* Window title includes the filename and a `*` dirty indicator.
* Closing with unsaved changes shows an internal `Save / Discard / Cancel` modal
  (the same flow as `ResourceBrowser`'s dirty-close modal).
* File-dialog filter includes `emesh` (the stash change that was on the old branch).

### Modified: `ResourcePanelRegistry` (`editor/ResourcePanelRegistry.h/.cpp`)

Added a third registration path for **external-window** extensions:

* `RegisterOpenCallback(extension, callback)` — stores an `OpenCallback` instead
  of a `PanelFactory`. The extension is still added to `extensions_` so it
  appears in the `ResourceBrowser` tree.
* `HasOpenCallback(path)` — returns `true` when the path's extension has a
  callback registered.
* `InvokeOpenCallback(path)` — calls the registered callback.
* `CanOpen(path)` continues to return `true` for both panel-factory and
  open-callback extensions (both appear in the tree).
* `Open(path)` now returns `nullptr` (rather than crashing) for open-callback
  extensions.

### Modified: `ResourceBrowser` (`editor/ResourceBrowser.cpp`)

`OpenOrFocus` now checks `registry_->HasOpenCallback(path)` first. If true,
it calls `registry_->InvokeOpenCallback(path)` and returns early — the
existing panel-tab flow is unchanged for all other extensions.  The new-file
modal's `OpenOrFocus(created)` call automatically inherits this behaviour for
newly created `.vehicle.yaml` files.

### Modified: `EditorWindow` (`editor/EditorWindow.h/.cpp`)

* Added `std::unique_ptr<VehicleEditorWindow> vehicle_editor_` member (forward
  declaration sufficient; it is a pointer member).
* Constructor now instantiates `vehicle_editor_`, calls
  `SetOnPlaceInScene(...)` on it, and registers the open callback via
  `resource_panel_registry_.RegisterOpenCallback(".vehicle.yaml", ...)`.
  The `RegisterNew` factory (skeleton file creation) is unchanged.
* `Render()` calls `vehicle_editor_->Render()` unconditionally — the window
  self-gates on its internal `show_` flag.
* Old `Register(".vehicle.yaml", ...)` panel-factory removed.

### Deleted: `VehiclePanel.h`, `VehiclePanel.cpp`

All logic migrated to `VehicleEditorWindow`.

### Modified: `editor/CMakeLists.txt`

`VehiclePanel.cpp` → `VehicleEditorWindow.cpp`.

## Decisions

* **Unconditional `Render()` call** — unlike `ParticleEditorWindow` /
  `SoundEditorWindow` which require an external `show_*` flag in `EditorWindow`,
  `VehicleEditorWindow` manages its own `show_` state internally. This avoids a
  redundant boolean in `EditorWindow` and is simpler since the window opens only
  from `ResourceBrowser` (not from a menu item that might need toggling).

* **`RegisterOpenCallback` as a first-class registry concept** — rather than a
  special-case in `ResourceBrowser`, the dispatch lives in the registry. This
  keeps `ResourceBrowser` policy-free and makes it trivial to promote future
  resource types to dedicated windows.

* **No `show_vehicle_editor_` in `EditorWindow`** — the issue spec showed this
  flag but the accompanying text said "Render() is called unconditionally". The
  self-contained approach is cleaner and consistent with how `MaterialEditorWindow`
  and `MeshEditorWindow` work (they call `Render()` unconditionally without an
  external flag).

## Skills and instructions used

* `impl-issue` skill
* `editor/CLAUDE.md` — one class per file, no inline editing logic in panels,
  GUI/edition separation, value-member include rule, StlAlgorithm preference
* `src/CLAUDE.md` — Google C++ style, include-root rule
* Root `CLAUDE.md` — cpplint pass, conventional commit, history file, PR to dev
