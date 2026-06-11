# Editor particle system integration (issue #455)

## Summary

Integrates `GameParticleSystem` objects fully into the editor: object panel, properties panel, gizmo rendering, creation workflow, resource panel listing, and per-frame simulation in the editor viewport.

## Changes

### New files

| File | Purpose |
|------|---------|
| `src/editor/ParticleSystemSelectionModal.h/.cpp` | Modal dialog listing all `*.particles.yaml` files found in `data/particles/` on `Open()`. Returns the selected template basename on confirm. Mirrors `MeshSelectionModal`. |
| `src/editor/ParticleGizmoRenderer.h/.cpp` | Renders three orthogonal wireframe circles (XY, XZ, YZ) at each `kParticleSystem` object's world position without depth testing. Radius comes from the first sub-system's `emitter_radius` (falls back to 0.5 m when the shape is `kPoint`). Mirrors `PlayerStartGizmoRenderer`. |

### Modified files

| File | Change |
|------|--------|
| `src/editor/EditorTool.h` | Added `kCreateParticleSystem` to the enum and `IsCreationTool()`. |
| `src/editor/EditorToolbar.cpp` | Added fire-icon toolbar button for `kCreateParticleSystem`. |
| `src/editor/ObjectsPanel.cpp` | Added `kParticleSystem` group with `ICON_FA_FIRE` icon. |
| `src/editor/PropertiesPanel.h/.cpp` | Added `RenderParticleSystemProperties()`, `SetOnOpenParticleEditor` callback, and dispatch in `Render()`. Shows template name (read-only) and an "Open in Particle Editor" button. |
| `src/editor/EditorScene.h/.cpp` | Added `Update(float time, float dt)` that ticks all `kParticleSystem` objects each editor frame so effects run live. |
| `src/editor/ResourcesPanel.h/.cpp` | Added "Particle Effects" section populated from `ParticleSystemTemplate::GetAll()`. Double-click fires `on_particle_open_`. |
| `src/editor/EditorViewport.h/.cpp` | Added `ParticleGizmoRenderer particle_gizmo_` member, `SetPendingParticleSystem(const std::string&)` placement entry-point, gizmo render call, and screen-space proximity picking for `kParticleSystem` objects. |
| `src/editor/EditorWindow.h/.cpp` | Added `ParticleSystemSelectionModal particle_modal_`, wired `on_particle_open_` and `on_open_particle_editor` callbacks, `scene_->Update()` call each frame, and `kCreateParticleSystem` tool dispatch in the tool-transition block. |
| `src/editor/ParticleEditorWindow.h/.cpp` | Added `OpenTemplate(const std::string& name)` to load a specific template by name without needing a file dialog. |
| `src/editor/CMakeLists.txt` | Added `ParticleGizmoRenderer.cpp` and `ParticleSystemSelectionModal.cpp`. |

## Decisions

- **`ICON_FA_FIRE`** was chosen for the particle system icon as it is available in the FontAwesome 6 free set and visually communicates "effects", consistent with other icons in the toolbar and object panel.
- **Radius fallback to 0.5 m** when `emitter_shape == kPoint` avoids rendering an invisible zero-radius sphere while staying small enough not to obscure dense scenes.
- **Three orthogonal circles** (not a full sphere triangulation) keeps the gizmo visually clear and GPU-cheap (3 × 32 = 96 line segments per object).
- **Screen-space proximity picking** (same threshold as player starts, 12 px) is used since particle systems have no solid geometry to ray-cast against.
- **`ParticleSystemSelectionModal` scans the filesystem** rather than using `GetAll()` so users can pick effects that have not yet been loaded into the current map.
- **`EditorScene::Update`** is called from `EditorWindow::Render()` (before `ImGui::DockSpaceOverViewport()`) so emitters advance every frame that the editor is running, independent of game-mode simulation.
- **`OpenTemplate`** in `ParticleEditorWindow` reloads the template via `GetOrLoad` and releases the caller's reference; the working copy (`sub_systems_`, `lights_`) is a by-value snapshot so edits do not mutate the live template.

## Output to keep in mind

- `GameParticleSystem` update in the editor now runs every frame; particle effects will be live-animated while the editor is open.
- The `ParticleSystemSelectionModal` scans `data/particles/` on every `Open()` call, so newly saved effect files are immediately available without restarting the editor.
- All new connections follow the callback pattern already established for materials and meshes (no back-pointers from panels to `EditorWindow`).

## Skills and CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; Google C++ style; include root is `src/`.
- `src/editor/CLAUDE.md`: editor is the dependency-graph leaf; one class per file; no singletons in subsystems.
- Conventional commits format: `feat(editor): …`.
