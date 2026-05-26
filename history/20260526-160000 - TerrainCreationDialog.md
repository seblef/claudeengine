# Terrain Creation Dialog — Issue #294

## Summary

Implemented `TerrainCreationDialog`: an ImGui modal dialog that lets the user
add a terrain to the current scene from the editor. Wired it into `EditorWindow`
via an "Add > Terrain" menu item in the main menu bar.

## Changes

### `src/editor/TerrainCreationDialog.h` (new)

Declares a non-singleton modal dialog class with:
- `Open()` — schedules the popup for the next frame
- `Render()` — draws the modal; returns `true` on the frame the user confirms
- Nested `Params` struct with five fields: `size_x`, `size_z`, `resolution`,
  `min_height`, `max_height`

### `src/editor/TerrainCreationDialog.cpp` (new)

Implements the dialog body:
- Five `ImGui::DragFloat` widgets for the parameters
- Derived info line: `"Heightmap: NNN × NNN texels — X.X MB"`
- Two orange warning lines: one if the heightmap exceeds 256 MB, one if
  resolution < 0.5 m/texel
- Create / Cancel buttons

### `src/editor/EditorWindow.h`

- Includes `TerrainCreationDialog.h`
- Forward-declares `terrain::TerrainNormalMap`
- Adds `TerrainCreationDialog terrain_dialog_` (value member)
- Adds `std::unique_ptr<terrain::TerrainNormalMap> terrain_normal_map_`
- Adds private `CreateTerrain()` method declaration

### `src/editor/EditorWindow.cpp`

- **"Add" menu** — new menu item in the main menu bar. Disables the "Terrain"
  item when a terrain already exists in the scene (with a tooltip). Opening
  it when the scene has no terrain calls `terrain_dialog_.Open()`.
- **Terrain dialog render** — `terrain_dialog_.Render()` is called each frame;
  when it returns `true`, `CreateTerrain()` is invoked.
- **`CreateTerrain()` method** — five-step terrain creation flow:
  1. Allocates `TerrainData` (all `uint16_t` samples = 0 → world Y = `min_height`).
  2. Builds a minimal `YAML::Node` with one layer (`default/diffuse.png` albedo,
     `default/normal.png` normal, tiling = 8.0) and calls `TerrainMaterial::Load()`.
  3. Builds a `TerrainNormalMap` via `Build()`.
  4. Constructs `GameTerrain` named `"terrain"` and adds it to the scene via
     `AddDynamicObject()`. `OnAddedToScene()` calls `TerrainRenderer::Init()`
     (heightmap GPU upload) and `SetMaterial()` (splatmap binding).
  5. Uploads the normal map GPU texture via `TerrainNormalMap::Upload()`.
  Sets `scene_dirty_ = true` and logs a summary line.
- **Normal map reset** — `terrain_normal_map_.reset()` is called whenever the
  scene is replaced (New Map modal confirmation and LoadFromFile paths), so the
  GPU texture lifetime matches the scene.

### `src/editor/CMakeLists.txt`

Added `TerrainCreationDialog.cpp` to the `editor` static library source list.

## Decisions

- **"Add > Terrain" menu item, not toolbar tool** — terrain creation opens a
  full-parameter dialog (unlike click-to-place mesh/light), so adding it as an
  `EditorTool` enum value would be misleading. A dedicated "Add" menu item is
  the cleanest integration and requires no change to `EditorTool` or
  `EditorToolbar`.
- **Disable item when terrain already exists** — `TerrainRenderer::Instance().Init()`
  asserts if called twice. Rather than crashing, the menu item is grayed out
  (with a tooltip) when a `kTerrain` object is already in the scene.
- **`terrain_normal_map_` stored in `EditorWindow`** — `GameTerrain` and
  `TerrainRenderer` do not yet expose a `SetNormalMap` API, so the normal map
  is stored in `EditorWindow` for lifetime management. It is reset on scene
  replacement (New Map, Load) to prevent GPU resource leaks.
- **Default layer uses `default/diffuse.png` / `default/normal.png`** — these
  textures are guaranteed to exist in `data/textures/default/` and are already
  referenced as fallbacks in other engine systems (`renderer/Material.cpp`,
  `editor/MaterialEditorWindow.cpp`).
- **`YAML::Node` built programmatically** — avoids file I/O and keeps the
  creation path self-contained; `TerrainMaterial::Load()` reads the same YAML
  structure produced by `Save()`, so the format is stable.

## Output / Things to Remember

- `terrain_normal_map_` in `EditorWindow` is not connected to `TerrainRenderer`
  yet — there is no `SetNormalMap()` API. The normal map is built and uploaded
  to GPU but not bound to a shader slot. This is prepared for a future issue
  that extends `TerrainRenderer` to consume it at slot 1 (VS).
- The "Add > Terrain" guard checks all objects' `GetType()` against `kTerrain`;
  this means that if a serialized terrain is loaded (via `MapLoader`), the menu
  item will correctly remain disabled.
- The `GameObjectType::kTerrain` discriminator was already present from
  issue #292; no changes to the `game` module were needed.

## Skills / Instructions Used

- `src/CLAUDE.md` — one class per `.h/.cpp`, Google C++ style, include root `src/`
- `src/editor/CLAUDE.md` — leaf module, all ImGui in NewFrame/Render bracket,
  owned by `EditorSystem`, no singletons for dialog
- `src/terrain/CLAUDE.md` — dependency rules (terrain does not depend on editor)
- `src/game/CLAUDE.md` — `GameObjectType::kTerrain` discriminator, `AddDynamicObject`
  triggers `OnAddedToScene()`
