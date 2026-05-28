# Environment editor panel

**Issue:** #327
**Branch:** `feat/environment-editor-panel-327`

## Goal

Add an ImGui dockable panel ("Environment") in the editor that exposes all
`EnvironmentDesc` parameters with live preview of changes.

## Changes

### `src/environment/EnvironmentDesc.h`

Added `float turbidity = 2.f;` field. The issue spec requires a turbidity slider in
the Sky section, but this field was absent from the struct (issue #319 omitted it).
Default matches `SkyRenderer`'s own default.

### `src/environment/WorldTime.h/.cpp`

Added `SetTimeOfDay(float hours)` to allow the panel's time-of-day slider to jump
directly to any hour, bypassing the `Advance()` accumulator. Clamps to [0, 86400 s).

### `src/environment/WindSystem.h/.cpp`

Added `SetBaseDirection(const core::Vec3f&)` and `SetBaseStrength(float)` for
hot-updating wind parameters without rebuilding the `WindSystem`. Both are called
from the panel when the user drags the wind widgets.

### `src/game/MapLoader.cpp`

`ParseEnvironmentDesc` now reads `turbidity` from YAML.

### `src/editor/MapSerializer.cpp`

`EmitEnvironment` now emits `turbidity:` when it differs from the default.

### `src/editor/EnvironmentEditorPanel.h/.cpp` (new files)

The panel owns two non-singleton runtime objects:
- `std::unique_ptr<WorldTime> world_time_` — advanced via `Tick(dt)` each frame
  unless `time_paused_` is set.
- `std::unique_ptr<WindSystem> wind_system_` — registered with the Renderer.

Renderer singletons (`SkyRenderer`, `WaterRenderer`, `CloudRenderer`) are created
on demand when their checkbox is ticked and torn down when unticked. The panel
`SetContext()` reads the scene's `EnvironmentDesc` and rebuilds all live systems
from it on map load.

`Render()` returns `bool` (changed this frame) so `EditorWindow` can set
`scene_dirty_` only when something actually changed.

Sections:

| Section | Widgets | Live side-effect |
|---|---|---|
| Time | Time of day slider, Pause checkbox, Time scale drag | `WorldTime::SetTimeOfDay/SetTimeScale`, `Renderer::Set*SkyWorldTime` |
| Sky | Sky enabled checkbox, Turbidity slider | `SkyRenderer::SetTurbidity`, build/tear-down `SkyRenderer` |
| Clouds | Cloud enabled checkbox, Cloud density slider | `CloudRenderer` build/tear-down, `Renderer::SetCloudDensity` |
| Wind | Wind enabled checkbox, Wind angle slider, Wind strength drag | `WindSystem::SetBaseDirection/SetBaseStrength`, build/tear-down `WindSystem` |
| Water | Water enabled checkbox, Water level drag | `WaterRenderer::SetWaterLevel`, build/tear-down `WaterRenderer` |
| Trees | Trees enabled checkbox | `Renderer::SetTreeEnabled` |

### `src/editor/EditorScene.h`

Already had `EnvironmentDesc` field from issue #326.

### `src/editor/EditorWindow.h/.cpp`

- Added `EnvironmentEditorPanel environment_panel_` and `bool show_environment_panel_`.
- Added "Environment" toggle menu item under the "Map" menu.
- Calls `environment_panel_.Tick(ImGui::GetIO().DeltaTime)` at the top of every frame.
- Calls `environment_panel_.SetContext(scene_.get(), video_)` whenever a new scene is
  created or loaded (constructor, new-map modal, load-from-file).
- Renders the panel in an ImGui window and propagates `scene_dirty_` from the return value.

### `src/editor/CMakeLists.txt`

Added `EnvironmentEditorPanel.cpp` to the `editor` static library.

## Decisions and rationale

**Wind direction as compass angle, not XZ vector**  
A single `SliderFloat("Wind angle", 0–360°)` is easier to reason about than dragging
two correlated float components. The angle is converted to/from a normalised `Vec3f`
on the way in and out.

**`turbidity` added to `EnvironmentDesc`**  
The field was referenced by `SkyRenderer::SetTurbidity()` and the issue spec, but
missing from the struct. Adding it here makes persistence automatic via the existing
load/save path.

**Panel owns WorldTime and WindSystem; singletons are shared**  
`WorldTime` and `WindSystem` are plain classes — fine to own by the panel.
Singletons (`SkyRenderer`, `WaterRenderer`, `CloudRenderer`) are shared with other
renderer passes, so the panel creates/destroys them on behalf of the whole renderer.

**`Render()` returns bool changed**  
Matches the pattern used by `MapPropertiesWindow::RenderPanel()` — avoids marking the
scene dirty on every frame when the panel is merely open.

## Output to keep in mind for next features

- `turbidity` is now persisted in the YAML `environment:` block — map files saved
  before this change will default to `2.0`.
- `SetTimeOfDay()` can be used by any future feature that needs to jump the in-game
  clock (e.g. time-lapse preview, sunrise at map open).
- `WindSystem::SetBaseDirection/SetBaseStrength` are now available for any future
  in-game wind event (gusts, weather changes).
- `EnvironmentEditorPanel::SetContext()` must be called whenever the `EditorScene`
  pointer is replaced; currently wired in constructor, new-map modal, and load-file
  path.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — Google C++ style, one class per file, project-relative includes
- `src/editor/CLAUDE.md` — editor is the leaf; no editor code in game/renderer
- `src/environment/CLAUDE.md` — environment module must not depend on renderer
- `src/game/CLAUDE.md` — game module dependency constraints
