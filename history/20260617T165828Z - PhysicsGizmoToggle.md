# Physics Gizmo Toggle — Rendering Settings Panel

**Issue**: #571  
**Branch**: `feat/physics-gizmo-toggle-571`  
**PR**: #618

## Summary

Adds a **Physics shapes** toggle to the editor so the user can show/hide physics wireframe gizmos for the selected mesh. A new `RenderingSettingsPanel` class owns the toggle and lives in the **View > Rendering settings** dockable panel.

## Files created / modified

| File | Change |
|---|---|
| `src/editor/RenderingSettingsPanel.h` | New — panel class with `IsPhysicsGizmosEnabled()` accessor |
| `src/editor/RenderingSettingsPanel.cpp` | New — single `ImGui::Checkbox` in `Render()` |
| `src/editor/CMakeLists.txt` | Added `RenderingSettingsPanel.cpp` to `editor` library |
| `src/editor/EditorViewport.h` | Added `SetRenderingSettingsPanel()` and `rendering_settings_panel_` member |
| `src/editor/EditorViewport.cpp` | Added physics gizmo enqueue between `PlayerStartGizmos` and `WireframeRenderer::Render()` |
| `src/editor/EditorWindow.h` | Added `rendering_settings_panel_` and `show_rendering_settings_panel_` |
| `src/editor/EditorWindow.cpp` | Wires panel into viewport; adds View menu item; renders panel |

## Design decisions

### New class vs. inline menu toggle

The terrain wireframe is a bare boolean toggled inline from the Debug menu. For physics gizmos the issue required a class with explicit `IsPhysicsGizmosEnabled()` accessor so the viewport can query it without depending on `EditorWindow` — consistent with how other panels are surfaced (non-owning pointer injected at construction).

### Panel placement (View vs. Debug menu)

The issue groups this with "gizmo/wireframe toggles." Putting it under **View** (alongside Post-process) keeps the menu clean; Debug is for developer-only raw overlays. A `RenderingSettingsPanel` panel also provides a natural container for future display-layer toggles.

### Call site in EditorViewport::Render()

The enqueue must happen after `Renderer::Update()` (which calls `WireframeRenderer::BeginFrame()`) and before `WireframeRenderer::Render()`. It is placed immediately after `EnqueuePlayerStartGizmos`, following the same pattern.

### Guard against nullptr

`rendering_settings_panel_` starts as `nullptr`; the enqueue block is guarded by `rendering_settings_panel_ && ...` so existing tests and any code path that creates a viewport without wiring the panel remain safe.

## What to keep in mind for next features

- If additional per-frame visual toggles are needed (e.g., "show bounding boxes", "show normals"), they belong in `RenderingSettingsPanel::Render()` with matching accessors.
- The physics gizmo only fires for the **selected** mesh. If a "show all physics shapes" mode is ever needed, a scene-wide loop would be required.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; include root is `src/`.
- `src/editor/CLAUDE.md`: editor is the dependency-graph leaf; panels are pure UI; non-owning pointer injection pattern for cross-panel wiring.
- Conventional commits format for commit message.
