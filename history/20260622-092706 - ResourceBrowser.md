# ResourceBrowser & IResourcePanel Framework (issue #709)

## What was done

Added the editor infrastructure for authoring resource descriptor files (`.vehicle.yaml`, `.vfx.yaml`, etc.) as part of milestone M1 — Play-in-Editor.

### New files

| File | Purpose |
|---|---|
| `editor/IResourcePanel.h` | Header-only interface: `Draw()`, `IsDirty()`, `Save()` |
| `editor/ResourcePanelRegistry.h/.cpp` | Maps compound extension suffixes to panel factories |
| `editor/ResourceBrowser.h/.cpp` | "Browser" tab in the Scene panel; scans `data/`, shows a tree, opens panels as tabs |

### Modified files

| File | Change |
|---|---|
| `editor/EditorWindow.h` | Added `ResourcePanelRegistry resource_panel_registry_` and `std::unique_ptr<ResourceBrowser> resource_browser_` members; added `#include "editor/ResourcePanelRegistry.h"` (value member requires full type) |
| `editor/EditorWindow.cpp` | Constructs `resource_browser_` at end of constructor; adds "Browser" tab to the Scene panel's tab bar |
| `editor/CMakeLists.txt` | Added `ResourceBrowser.cpp` and `ResourcePanelRegistry.cpp` |

## Decisions and rationale

### Header include vs. forward declaration for `ResourcePanelRegistry`
`resource_panel_registry_` is a **value member** in `EditorWindow`, so the full type must be visible in the header. A forward declaration alone is insufficient. We include `ResourcePanelRegistry.h` directly in `EditorWindow.h`. `ResourceBrowser` is a `unique_ptr` so it only needs a forward declaration in the header.

### Extension matching: compound suffixes
The registry iterates suffixes in registration order and checks `filename.compare(…)` against the full filename. This correctly handles `.vehicle.yaml` vs `.yaml`: the first registered suffix that matches wins, so more specific suffixes should be registered first.

### Scan on construction + Refresh button
The scan happens eagerly in `ResourceBrowser`'s constructor. A "Refresh" button re-triggers it. This is sufficient for M1 where no concrete panels exist yet; a file-watcher could be added later.

### Dirty-close protection
Mirrors `EditorWindow::RenderUnsavedChangesModal` exactly (Save / Discard / Cancel buttons in a modal). The pending close index is stored; the modal resolves it on the next frame.

### Ctrl+S scope
`Ctrl+S` inside `ResourceBrowser::RenderPanelTabs` only fires when the global `WantCaptureKeyboard` is false — same guard used in `EditorWindow::RenderMenuBar`. This avoids conflict with text input widgets inside future concrete panels.

### `active_tab_` tracking
The browser tracks the focused tab by index into `open_panels_`. On erase, the index is clamped. `ImGuiTabItemFlags_SetSelected` is used to focus a tab programmatically when `OpenOrFocus` is called for an already-open panel.

## Output to keep in mind for future features

- **Concrete panels**: add a `registry.Register(".vehicle.yaml", factory)` call in `EditorWindow`'s constructor before `resource_browser_` is constructed. The browser will automatically show the new section.
- **No concrete `IResourcePanel` implementation exists yet**. The first one (`VehiclePanel`) arrives in M2.
- The `ResourceBrowser` does not replace `ResourcesPanel` — they are separate tabs serving different purposes (in-memory assets vs. descriptor files on disk).

## Skills and CLAUDE.md instructions followed

- Skill: `impl-issue` — branch from `dev`, implement, cpplint, commit, PR to `dev`.
- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; Google C++ style; include root is `src/`.
- `editor/CLAUDE.md`: pure UI panel (no business logic inline in Render methods); one class per file; ImGui calls bracketed correctly.
